#include "WarlockGame.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "WarlockAnimation.h"
#include "WarlockAppearance.h"
#include "WarlockTuning.h"
#include "WarlockSpellFX.h"

namespace
{
UStaticMesh* Shape(const TCHAR* Path) {return LoadObject<UStaticMesh>(nullptr,Path);}
void SetMaterial(UStaticMeshComponent* Mesh,const TCHAR* Path,FLinearColor Color)
{
    if(auto Base=LoadObject<UMaterialInterface>(nullptr,Path))
    {auto M=UMaterialInstanceDynamic::Create(Base,Mesh);M->SetVectorParameterValue(TEXT("Tint"),Color);Mesh->SetMaterial(0,M);}
}
FLinearColor TeamColor(int I)
{
    const FLinearColor Colors[]={FLinearColor(.12f,.65f,1),FLinearColor(1,.19f,.12f),FLinearColor(.6f,.22f,1),FLinearColor(.1f,1,.5f)};
    return Colors[FMath::Abs(I)%4];
}
float EffectiveRange(const AWarlockState* S){return 1+.12f*(S->RangeMastery-S->Items[7]);}
float Duration(const AWarlockState* S){return 1+.08f*S->TimeMastery-.05f*S->Items[0];}
float TargetLeech(const AWarlockState* S)
{
    static const float Ring[]={0,.06f,.1f,.1f};
    return 1+(S->Items[1]? .05f:0)+Ring[S->Items[5]]+.05f*S->Items[6];
}
}
AWarlockPawn::AWarlockPawn()
{
    PrimaryActorTick.bCanEverTick=true; bReplicates=true;SetReplicateMovement(true); SetNetUpdateFrequency(33);
    Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));SetRootComponent(Body);Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetStaticMesh(Shape(TEXT("/Engine/BasicShapes/Cone.Cone")));Body->SetRelativeScale3D(FVector(1));
    Avatar=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WarlockAvatar"));Avatar->SetupAttachment(Body);
    Avatar->SetCollisionEnabled(ECollisionEnabled::NoCollision);Avatar->SetRelativeLocation(FVector(0,0,-40));Avatar->SetRelativeScale3D(FVector(.92f));
    Avatar->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Skin(TEXT("/Game/WarlockArena/Characters/PolishedWarlock/Warlock/SkeletalMeshes/Warlock.Warlock"));
    Avatar->SetSkeletalMeshAsset(Skin.Object);Avatar->SetAnimInstanceClass(UWarlockAnimation::StaticClass());
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/WarlockArena/Characters/PolishedWarlock/Warlock/SkeletalMeshes/WarlockStand.WarlockStand"));IdleAnimation=Idle.Object;
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(TEXT("/Game/WarlockArena/Characters/PolishedWarlock/Warlock/SkeletalMeshes/WarlockWalk.WarlockWalk"));WalkAnimation=Walk.Object;
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack(TEXT("/Game/WarlockArena/Characters/PolishedWarlock/Warlock/SkeletalMeshes/WarlockAttack.WarlockAttack"));AttackAnimation=Attack.Object;
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Spell(TEXT("/Game/WarlockArena/Characters/PolishedWarlock/Warlock/SkeletalMeshes/WarlockSpell.WarlockSpell"));SpellAnimation=Spell.Object;
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Slam(TEXT("/Game/WarlockArena/Characters/PolishedWarlock/Warlock/SkeletalMeshes/WarlockAttack_Slam.WarlockAttack_Slam"));SlamAnimation=Slam.Object;
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Death(TEXT("/Game/WarlockArena/Characters/PolishedWarlock/Warlock/SkeletalMeshes/WarlockDeath.WarlockDeath"));DeathAnimation=Death.Object;
    Crown=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AppearanceCrown"));Crown->SetupAttachment(Avatar,TEXT("37_Bone_Head"));Crown->SetCollisionEnabled(ECollisionEnabled::NoCollision);Crown->SetRelativeLocation(FVector(0,0,12));Crown->SetRelativeScale3D(FVector(.62f));
    for(const TCHAR* Name:{TEXT("SM_CrimsonCrown"),TEXT("SM_MoonCrown"),TEXT("SM_GroveCrown")})CrownMeshes.Add(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/WarlockArena/Characters/Ornaments/%s.%s"),Name,Name)));
    for(const TCHAR* Name:{TEXT("M_SorcererCloth_Natural"),TEXT("M_SorcererMetal_Natural"),TEXT("M_SorcererSkin_Natural"),TEXT("M_SorcererGlow_Natural")})AppearanceMaterials.Add(LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/WarlockArena/Characters/Materials/%s.%s"),Name,Name)));
    Hat=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hat"));Hat->SetupAttachment(Body);Hat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Hat->SetStaticMesh(Shape(TEXT("/Engine/BasicShapes/Cone.Cone")));Hat->SetRelativeLocation(FVector(0,0,80));Hat->SetRelativeScale3D(FVector(.8f,.8f,.55f));
    Halo=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rune"));Halo->SetupAttachment(Body);Halo->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Halo->SetStaticMesh(Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));Halo->SetRelativeLocation(FVector(0,0,-38));Halo->SetRelativeScale3D(FVector(.72f,.72f,.008f));
    ShieldMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReflectShield"));ShieldMesh->SetupAttachment(Body);ShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ShieldMesh->SetStaticMesh(Shape(TEXT("/Engine/BasicShapes/Sphere.Sphere")));ShieldMesh->SetUsingAbsoluteScale(true);ShieldMesh->SetWorldScale3D(FVector(2.4f));ShieldMesh->SetVisibility(false);
    CameraArm=CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));CameraArm->SetupAttachment(Body);CameraArm->SetUsingAbsoluteRotation(true);CameraArm->SetUsingAbsoluteScale(true);
    CameraArm->SetWorldScale3D(FVector(1));CameraArm->SetRelativeRotation(FRotator(-65,-90,0));CameraArm->TargetArmLength=2000;CameraArm->bDoCollisionTest=false;
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));Camera->SetupAttachment(CameraArm);Camera->FieldOfView=65;
    Camera->bOverrideAspectRatioAxisConstraint=true;Camera->AspectRatioAxisConstraint=AspectRatio_MaintainYFOV;
    Cooldowns.Init(0,8);
    AppearanceData=TSoftObjectPtr<UWarlockAppearanceSet>(FSoftObjectPath(TEXT("/Game/WarlockArena/Characters/DA_WarlockAppearances.DA_WarlockAppearances")));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Wizard(TEXT("/Game/WarlockArena/Materials/M_Wizard.M_Wizard"));
    WizardMaterial=Wizard.Object;
}
AWarlockState* AWarlockPawn::State() const {return GetPlayerState<AWarlockState>();}
float AWarlockPawn::MaxHealth() const {auto S=State();return 100+(S?S->Items[6]*10:0);}
float AWarlockPawn::MoveSpeed() const
{
    auto S=State();if(!S)return 210;static const float Lava[]={0,13,24,35},Speed[]={0,20,30,40};return 210+Lava[S->Items[1]]+Speed[S->Items[4]]+HasteSpeed;
}
float AWarlockPawn::KnockbackScale() const
{
    auto S=State();if(!S)return 1;static const float Helmet[]={1,.84f,.76f,.68f};return (1-.025f*(S->LifeMastery+S->RangeMastery+S->TimeMastery))*Helmet[S->Items[7]];
}
bool AWarlockPawn::IsEnemy(const AWarlockPawn* Other) const
{
    return Other&&Other!=this&&State()&&Other->State()&&State()->Team!=Other->State()->Team;
}
void AWarlockPawn::BeginPlay()
{
    Super::BeginPlay();if(HasAuthority())if(auto M=GetWorld()->GetAuthGameMode<AWarlockMode>())M->Fighters.AddUnique(this);
    SetMaterial(ShieldMesh,TEXT("/Game/WarlockArena/Materials/M_Shield.M_Shield"),FLinearColor(.15f,.5f,1));
}
void AWarlockPawn::Tick(float Dt)
{
    Super::Tick(Dt);
    if(State())
    {
        auto Color=TeamColor(State()->Team);
        if(!WizardMaterial){WizardMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WarlockArena/Materials/M_Wizard.M_Wizard"));}
        if(!bAppearanceReady&&WizardMaterial)
        {
            auto M=UMaterialInstanceDynamic::Create(WizardMaterial,this);M->SetVectorParameterValue(TEXT("Tint"),Color);Body->SetMaterial(0,M);Hat->SetMaterial(0,M);
            SetMaterial(Halo,TEXT("/Game/WarlockArena/Characters/Materials/M_SorcererRune.M_SorcererRune"),Color*.6f);bAppearanceReady=true;
        }
    }
    const bool HasAvatar=Avatar&&Avatar->GetSkeletalMeshAsset();
    Body->SetVisibility(bAlive&&!HasAvatar);Hat->SetVisibility(bAlive&&!HasAvatar);
    if(bAlive)DeathVisualAge=0;else DeathVisualAge+=Dt;
    Avatar->SetVisibility(bAlive||DeathVisualAge<3);Crown->SetVisibility(HasAvatar&&(bAlive||DeathVisualAge<3));
    Halo->SetVisibility(bAlive);ShieldMesh->SetVisibility(bAlive&&(ShieldRemaining>0||Protection>0));
    if(State()&&HasAvatar&&AppliedAppearance!=State()->Appearance)
    {
        AppliedAppearance=FMath::Clamp(State()->Appearance,0,2);
        const FLinearColor Cloth[]={FLinearColor(.09f,.036f,.032f),FLinearColor(.035f,.055f,.08f),FLinearColor(.035f,.065f,.045f)};
        const FLinearColor Metal[]={FLinearColor(.26f,.19f,.11f),FLinearColor(.26f,.29f,.31f),FLinearColor(.16f,.19f,.12f)};
        const FLinearColor Glow[]={FLinearColor(.55f,.16f,.025f),FLinearColor(.09f,.25f,.4f),FLinearColor(.12f,.32f,.15f)};
        auto Palette=AppearanceData.LoadSynchronous();const FWarlockLook* Look=Palette&&Palette->Looks.IsValidIndex(AppliedAppearance)?&Palette->Looks[AppliedAppearance]:nullptr;
        FLinearColor ClothTint=Look?Look->Cloth:Cloth[AppliedAppearance],MetalTint=Look?Look->Metal:Metal[AppliedAppearance],GlowTint=Look?Look->Glow:Glow[AppliedAppearance];
        auto Mesh=Avatar->GetSkeletalMeshAsset();
        for(int I=0;I<Mesh->GetMaterials().Num();++I)
        {
            FString Slot=Mesh->GetMaterials()[I].MaterialSlotName.ToString();int MaterialRole=0;FLinearColor Tint=ClothTint;
            if(Slot.Contains(TEXT("Copper"))||Slot.Contains(TEXT("PBR_27_"))||Slot.Contains(TEXT("armorpads"))){MaterialRole=1;Tint=MetalTint;}
            if(Slot.Contains(TEXT("PBR_05_"))){MaterialRole=0;Tint=ClothTint*.35f+FLinearColor(.025f,.018f,.012f,0);}
            if(Slot.Contains(TEXT("Sylvanus"))){MaterialRole=2;Tint=Palette?Palette->SkinTone:FLinearColor(.35f,.24f,.19f);}
            if(Slot.Contains(TEXT("Black32"))){MaterialRole=0;Tint=FLinearColor(.045f,.033f,.028f);}
            if(Slot.Contains(TEXT("Water"))||Slot.Contains(TEXT("Glow"))){MaterialRole=3;Tint=GlowTint;}
            if(AppearanceMaterials.IsValidIndex(MaterialRole)&&AppearanceMaterials[MaterialRole]){auto Material=UMaterialInstanceDynamic::Create(AppearanceMaterials[MaterialRole],this);Material->SetVectorParameterValue(TEXT("Tint"),Tint);Avatar->SetMaterial(I,Material);}
        }
        Crown->SetStaticMesh(CrownMeshes[AppliedAppearance]);
        if(AppearanceMaterials[1]){auto Mat=UMaterialInstanceDynamic::Create(AppearanceMaterials[1],this);Mat->SetVectorParameterValue(TEXT("Tint"),MetalTint);Crown->SetMaterial(0,Mat);}
    }
    if(auto M=GetWorld()->GetGameState<AWarlockMatchState>())
    {
        // Stable world orientation for both local players; a wider view for split screens.
        CameraArm->TargetArmLength=FMath::FInterpTo(CameraArm->TargetArmLength,CameraDistance,Dt,5);
        if(auto PC=::Cast<APlayerController>(GetController());PC&&PC->IsLocalController()&&!bAlive&&M->Phase!=TEXT("SHOP"))
        {
            AWarlockPawn* Target=nullptr;
            for(TActorIterator<AWarlockPawn> It(GetWorld());It;++It)if(It->bAlive&&It->State())
            {if(!Target)Target=*It;if(State()&&It->State()->Team==State()->Team){Target=*It;break;}}
            Spectating=Target;
            if(Target)CameraArm->TargetOffset=FMath::VInterpTo(CameraArm->TargetOffset,Target->GetActorLocation()-GetActorLocation(),Dt,5);
        }
    }
    if(HasAuthority()&&bAlive&&ChainTime>0&&!ChainVisual.IsValid())ChainVisual=AWarlockSpellFX::Spawn(GetWorld(),6,GetActorLocation(),ChainAnchor,100,this,7);
}
void AWarlockPawn::ResetForRound(FVector Position)
{
    CancelCast();
    Health=MaxHealth();Knockback=0;bAlive=true;Momentum=MoveInput=FVector::ZeroVector;Cooldowns.Init(0,8);ShieldRemaining=0;SecondBlink=0;
    LocomotionSpeed=0;LastCastSlot=255;++CastSerial;FacingHold=0;
    Spectating.Reset();CameraArm->TargetOffset=FVector::ZeroVector;
    ChainTime=ShotsLeft=0;ChainTarget.Reset();Contributions.Empty();LastAttacker.Reset();
    auto Match=GetWorld()->GetGameState<AWarlockMatchState>();Protection=Match&&Match->Phase==TEXT("FIGHT")?4.f:0.f;
    LavaProtection=BootsCooldown=0;bClickMoving=false;
    HasteRemaining=HastePool=HasteSpeed=0;bPendingSecondBlink=false;
    SetActorLocation(Position+FVector(0,0,40));AimPoint=FVector::ZeroVector;ForceNetUpdate();
}
void AWarlockPawn::StepSimulation(float Dt)
{
    if(!HasAuthority()||!bAlive||!FMath::IsFinite(Dt)||Dt<=0)return;
    for(auto& C:Cooldowns)C=FMath::Max(0.f,C-Dt);
    Protection=FMath::Max(0.f,Protection-Dt);ShieldRemaining=FMath::Max(0.f,ShieldRemaining-Dt);
    SecondBlink=FMath::Max(0.f,SecondBlink-Dt);LavaProtection=FMath::Max(0.f,LavaProtection-Dt);BootsCooldown=FMath::Max(0.f,BootsCooldown-Dt);
    HasteRemaining=FMath::Max(0.f,HasteRemaining-Dt);if(HasteRemaining<=0)HastePool=HasteSpeed=0;
    auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();if(!M)return;
    if(PendingCast>=0)
    {
        CastRemaining=FMath::Max(0.f,CastRemaining-Dt);
        if(CastRemaining<=1.e-5f)
        {int Slot=PendingCast;PendingCast=-1;AimPoint=CastTarget;ReleaseSpell(Slot);ForceNetUpdate();}
    }
    if(ShotsLeft>0)
    {
        ShotTimer-=Dt;
        while(ShotTimer<=0&&ShotsLeft>0)
        {
            SpraySign=-SpraySign;
            float Angle=FMath::RadiansToDegrees(.32f*SpraySign*float(ShotsLeft-2)/FMath::Max(1,ShotsTotal));
            auto Direction=(SprayTarget-GetActorLocation()).GetSafeNormal2D().RotateAngleAxis(Angle,FVector::UpVector);
            M->Fire(this,Direction,2.3f+.3f*State()->Level(4),.6f,1200,EffectiveRange(State())/1.2f,1);
            --ShotsLeft;ShotTimer+=.08f;
        }
    }
    FVector Desired=MoveInput.GetClampedToMaxSize(1);
    if(bClickMoving){Desired=(ClickTarget-GetActorLocation()).GetSafeNormal2D();if(FVector::Dist2D(ClickTarget,GetActorLocation())<8){Desired=FVector::ZeroVector;bClickMoving=false;}}
    if(PendingCast>=0)Desired=FVector::ZeroVector;
    FVector Position=GetActorLocation();
    FacingHold=FMath::Max(0.f,FacingHold-Dt);
    FVector Facing=PendingCast>=0||FacingHold>0?CastTarget-Position:Desired.IsNearlyZero()?AimPoint-Position:Desired;
    if(!Facing.IsNearlyZero())SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(),FRotator(0,Facing.Rotation().Yaw,0),Dt,720));
    // ub decays the external part before integrating the current step.
    Momentum*=.96f;
    const FVector ExternalStep=Momentum;
    FVector Next=Position+Desired*MoveSpeed()*Dt+Momentum;
    if(ChainTime>0)
    {
        ++ChainSteps;
        auto Target=ChainTarget.Get();FVector End=Target?Target->GetActorLocation():ChainAnchor;
        if(Target&&!Target->bAlive){ChainTime=0;}
        else
        {
            FVector D=End-Position;float Distance=D.Size2D();
            float Minimum=Target?90.f:65.f;
            if(Distance<Minimum)ChainTime=0;
            else if(Target){auto N=D.GetSafeNormal2D();Target->Momentum-=N*1.4f;
                ChainDamageTimer-=Dt;if(ChainDamageTimer<=0){ChainDamageTimer+=.18f;int Ramp=FMath::Max(0,(ChainSteps-2)/5);float Damage=.1f+.1f*State()->Level(7)+FMath::Max(0,5-Ramp)/5.f;Target->Hit(this,Damage,-N,0);}}
            else Momentum+=D.GetSafeNormal2D()*1.4f;
        }
    }
    if(M->Arena)for(int I=0;I<M->Arena->Pillars.Num();++I)
    {
        if(M->Arena->PillarHealth[I]<=0)continue;
        FVector Delta=Next-M->Arena->Pillars[I];Delta.Z=0;
        if(Delta.SizeSquared()<FMath::Square(65.f))
        {auto Normal=Delta.GetSafeNormal();if(Normal.IsNearlyZero())Normal=FVector(1,0,0);Next=M->Arena->Pillars[I]+Normal*65;Next.Z=40;
            float Impact=Momentum.Size2D();if(Impact>20){float D=.7f*FMath::Sqrt(Impact-20);M->Arena->DamagePillar(I,D,this);Health-=D;if(Health<.5f)Die();}Momentum=FVector::VectorPlaneProject(Momentum,Normal)*.5f;}
    }
    Next.Z=40;Next.X=FMath::Clamp(Next.X,-5000.,5000.);Next.Y=FMath::Clamp(Next.Y,-5000.,5000.);
    LocomotionSpeed=Desired.IsNearlyZero()?0:FMath::Clamp(float(FVector::DotProduct(Next-Position-ExternalStep,Desired.GetSafeNormal2D()))/Dt,0.f,MoveSpeed());
    SetActorLocation(Next);
}
void AWarlockPawn::StepGround()
{
    auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();auto S=State();if(!HasAuthority()||!M||!S||!bAlive)return;
    if(GetActorLocation().SizeSquared2D()>FMath::Square(M->Match()->ArenaRadius))
    {
        if(Protection>0)return;
        float Damage=.9f*(LavaProtection>0?.125f:1);Health-=Damage;
        if(Health<.5f){Die();return;}Knockback+=Damage*.5f;
        if(auto A=LastAttacker.Get())Contributions.FindOrAdd(A)+=Damage*.5f;
    }
    else
    {
        static const float Regen[]={0,.02f,.035f,.045f};
        float RegenTick=(.05f+Regen[S->Items[5]]+(S->Items[6]==3?.01f:0))*(1+.05f*(S->LifeMastery+S->RangeMastery+S->TimeMastery));
        Health=FMath::Min(MaxHealth(),Health+RegenTick);
    }
}
bool AWarlockPawn::Cast(int Slot)
{
    auto S=State();auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();
    if(!HasAuthority()||!bAlive||!S||!M||M->Match()->Phase!=TEXT("FIGHT")||Slot<0||Slot>=8||S->Level(Slot)==0)return false;
    if(Slot==7&&ChainTime>0){ChainTime=0;ChainTarget.Reset();return true;}
    bool Second=Slot==3&&S->Modes[3]&&SecondBlink>0;
    if((Cooldowns[Slot]>.001f&&!Second)||PendingCast>=0)return false;
    bPendingSecondBlink=Second;
    CastTarget=AimPoint;PendingCast=Slot;
    CastDuration=CastRemaining=GetDefault<UWarlockTuning>()->Windup(Slot);
    LastCastSlot=Slot;++CastSerial;FacingHold=CastDuration+.25f;bClickMoving=false;
    FVector Facing=(CastTarget-GetActorLocation()).GetSafeNormal2D();if(!Facing.IsNearlyZero())SetActorRotation(FRotator(0,Facing.Rotation().Yaw,0));
    if(CastRemaining<=0){PendingCast=-1;ReleaseSpell(Slot);}
    if(CastRemaining>0)AWarlockSpellFX::Spawn(GetWorld(),7,GetActorLocation(),CastTarget,45,this,Slot);
    ForceNetUpdate();return true;
}
void AWarlockPawn::CancelCast()
{
    if(!HasAuthority())return;
    if(PendingCast>=0){LastCastSlot=255;++CastSerial;}
    PendingCast=-1;CastRemaining=CastDuration=0;FacingHold=0;ForceNetUpdate();
}
void AWarlockPawn::ReleaseSpell(int Slot)
{
    auto S=State();auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();
    if(!bAlive||!S||!M||M->Match()->Phase!=TEXT("FIGHT"))return;
    const int L=S->Level(Slot);const float Range=EffectiveRange(S);auto Position=GetActorLocation();auto D=(AimPoint-Position).GetSafeNormal2D();
    if(D.IsNearlyZero())D=FVector(1,0,0);
    bool Second=Slot==3&&bPendingSecondBlink;
    bPendingSecondBlink=false;
    if(Second&&SecondBlink<=0)return;
    if(!Second)Cooldowns[Slot]=WarlockRules::Cooldown(Slot,L,S->Modes[Slot]);
    switch(Slot)
    {
    case 0:M->Fire(this,D,7+.5f*(L-1),1,1000,Range);break;
    case 1:
        ChainTime=0;
        if(S->Items[1]&&BootsCooldown<=0&&Position.Size2D()>M->Match()->ArenaRadius)
        {LavaProtection=(2+S->Items[1])*Duration(S);BootsCooldown=28;Health=FMath::Max(Health,10.5f);M->Pulse(Position,100,1);break;}
        Health=FMath::Max(.5f,Health-10);
        for(auto P:M->Fighters)if(P->bAlive&&IsEnemy(P)){float Dist=FVector::Dist2D(Position,P->GetActorLocation());if(Dist<250){float Previous=P->Health;P->Hit(this,10+S->Items[2],P->GetActorLocation()-Position,1-Dist/1000);if(S->Items[2]&&P->Health<Previous)Knockback=FMath::Max(0.f,Knockback-(1+S->Items[2]));}}
        AWarlockSpellFX::Spawn(GetWorld(),0,Position,Position,250,this,1);break;
    case 2:
    {
        // Swept thin beam, first obstructing target or pillar.
        float Reach=600*Range;AWarlockPawn* Target=nullptr;int Pillar=-1;
        for(auto P:M->Fighters)if(P->bAlive&&IsEnemy(P))
        {FVector V=P->GetActorLocation()-Position;float Along=FVector::DotProduct(V,D);if(Along>0&&Along-30<Reach&&FVector::Dist2D(Position+D*Along,P->GetActorLocation())<50){Reach=FMath::Max(0.f,Along-30);Target=P;}}
        if(M->Arena)for(int I=0;I<M->Arena->Pillars.Num();++I)if(M->Arena->PillarHealth[I]>0)
        {auto V=M->Arena->Pillars[I]-Position;float Along=FVector::DotProduct(V,D);if(Along>0&&Along<Reach&&FVector::Dist2D(Position+D*Along,M->Arena->Pillars[I])<=35){Reach=Along;Target=nullptr;Pillar=I;}}
        AWarlockSpellFX::Spawn(GetWorld(),1,Position+FVector(0,0,55),Position+D*Reach+FVector(0,0,35),Reach,this,2);
        if(Target)Target->Hit(this,6+L,D,.92f);if(Pillar>=0)M->Arena->DamagePillar(Pillar,6+L,this);break;
    }
    case 3:
    {
        ++BlinkSerial;
        float Reach=S->Modes[3]?(Second?300.f:300.f+50*L):700.f+70*L;
        float Distance=FMath::Min(FVector::Dist2D(Position,AimPoint),Reach);FVector End=Position+D*Distance;
        if(M->Arena)for(int I=0;I<M->Arena->Pillars.Num();++I)if(M->Arena->PillarHealth[I]>0&&FVector::Dist2D(End,M->Arena->Pillars[I])<65)End=M->Arena->Pillars[I]+(End-M->Arena->Pillars[I]).GetSafeNormal2D()*66;
        End.Z=40;AWarlockSpellFX::Spawn(GetWorld(),2,Position,End,80,this,3);SetActorLocation(End);bClickMoving=false;
        if(S->Modes[3])SecondBlink=Second?0.f:3.3f;
        else {float V=Momentum.Size2D();if(V>12)Momentum*=FMath::Max(12.f,.8f*V)/V;}
        break;
    }
    case 4:
        if(S->Modes[4])for(int I=0;I<4;++I)M->Fire(this,D.RotateAngleAxis(6.9f*(I-1.5f),FVector::UpVector),3.3f+.3f*L,.65f,800,760.f*Range/900,1);
        else {ShotsLeft=ShotsTotal=5+L;ShotTimer=.08f;SprayTarget=AimPoint;}
        break;
    case 5:
    {FVector Target=Position+(AimPoint-Position).GetClampedToMaxSize2D(1000);Target.Z=40;M->Meteor(this,Target,L,false);break;}
    case 6:
        ChainTime=0;
        if(S->DefenseChoice==1){Cooldowns[6]=22.f-L;HasteRemaining=(5.5f+.5f*L)*Duration(S);HastePool=3+2*L;HasteSpeed=35;for(auto P:M->Fighters)if(P->ChainTarget.Get()==this){P->ChainTime=0;P->ChainTarget.Reset();}AWarlockSpellFX::Spawn(GetWorld(),9,Position,Position,100,this,6);}
        else {ShieldRemaining=(2.6f+.2f*L)*Duration(S);AWarlockSpellFX::Spawn(GetWorld(),5,Position,Position,120,this,6);}
        break;
    case 7:M->Fire(this,D,0,0,950,.86f*Range,2);break;
    }
}
void AWarlockPawn::Hit(AWarlockPawn* Attacker,float Damage,const FVector& Direction,float Push)
{
    if(!HasAuthority()||!bAlive||!FMath::IsFinite(Damage)||Direction.ContainsNaN()||!FMath::IsFinite(Push)||!Attacker||!Attacker->IsEnemy(this)||!State()||!Attacker->State())return;
    auto S=State();auto A=Attacker->State();
    // Preserve map's attacker + defender flat reduction until BUG01 intent is confirmed.
    // No flat-reduction item is enabled in the initial eight-item page.
    Damage=FMath::Max(0.f,Damage);
    float Outgoing=Attacker->Protection>0?.1f:1.f;
    float PushDamage=Damage*Outgoing;
    if(Damage<=0)return;
    Momentum+=Direction.GetSafeNormal2D()*WarlockRules::Impulse(PushDamage,Knockback,KnockbackScale()*Push*(Protection>0?.1f:1.f));
    // GN/mA: absorption changes HP/K damage after PN has computed impulse (BUG02).
    if(HasteRemaining>0&&HastePool>0){float Absorb=FMath::Min(HastePool,Damage*.5f);HastePool-=Absorb;HasteSpeed+=15*Absorb;Damage-=Absorb;}
    Damage*=Outgoing;
    // AN reduces outgoing damage and received impulse; GN suppresses HP/K
    // changes while protected, after PN has already added the reduced impulse.
    if(Protection>0){ForceNetUpdate();return;}
    Health-=Damage;Knockback+=Damage;LastAttacker=Attacker;Contributions.FindOrAdd(Attacker)+=Damage;
    float Heal=Damage*.1f*A->LifeMastery*TargetLeech(S);
    Attacker->Health=FMath::Min(Attacker->MaxHealth(),Attacker->Health+Heal);Attacker->Knockback=FMath::Max(0.f,Attacker->Knockback-Heal*.5f);
    if(auto M=GetWorld()->GetAuthGameMode<AWarlockMode>())M->Pulse(GetActorLocation(),45,0);
    if(Health<.5f)Die();
    ForceNetUpdate();Attacker->ForceNetUpdate();
}
void AWarlockPawn::Die()
{
    CancelCast();
    if(!bAlive)return;bAlive=false;Health=0;Momentum=MoveInput=FVector::ZeroVector;LocomotionSpeed=0;ShotsLeft=0;ShieldRemaining=0;ChainTime=0;LavaProtection=0;
    HasteRemaining=HastePool=HasteSpeed=0;
    auto S=State();auto Killer=LastAttacker.Get();
    if(Killer&&Killer->State()&&Killer->IsEnemy(this))Killer->State()->Points++;
    else if(S){if(auto M=GetWorld()->GetGameState<AWarlockMatchState>();M&&!M->bTeams)S->Points--;}
    AWarlockPawn* Assist=nullptr;float Best=0;
    for(const auto& Pair:Contributions)if(Pair.Key.IsValid()&&Pair.Key->IsEnemy(this)&&Pair.Value>Best){Assist=Pair.Key.Get();Best=Pair.Value;}
    if(Assist&&Assist->State())Assist->State()->Points++;
    ForceNetUpdate();
}
void AWarlockPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(AWarlockPawn,Health);DOREPLIFETIME(AWarlockPawn,Knockback);
    DOREPLIFETIME(AWarlockPawn,bAlive);DOREPLIFETIME(AWarlockPawn,ShieldRemaining);DOREPLIFETIME(AWarlockPawn,Cooldowns);DOREPLIFETIME(AWarlockPawn,Protection);
    DOREPLIFETIME(AWarlockPawn,LocomotionSpeed);DOREPLIFETIME(AWarlockPawn,CastSerial);DOREPLIFETIME(AWarlockPawn,LastCastSlot);
    DOREPLIFETIME(AWarlockPawn,PendingCast);DOREPLIFETIME(AWarlockPawn,CastRemaining);DOREPLIFETIME(AWarlockPawn,CastDuration);DOREPLIFETIME(AWarlockPawn,BlinkSerial);
    DOREPLIFETIME(AWarlockPawn,HasteRemaining);DOREPLIFETIME(AWarlockPawn,HastePool);DOREPLIFETIME(AWarlockPawn,HasteSpeed);
    DOREPLIFETIME(AWarlockPawn,SecondBlink);
}
AWarlockProjectile::AWarlockProjectile()
{
    PrimaryActorTick.bCanEverTick=true;bReplicates=true;SetReplicateMovement(true);SetNetUpdateFrequency(33);
    VisualRoot=CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));SetRootComponent(VisualRoot);
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Projectile"));Mesh->SetupAttachment(VisualRoot);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetStaticMesh(Shape(TEXT("/Engine/BasicShapes/Sphere.Sphere")));Mesh->SetRelativeScale3D(FVector(.5f));
}
void AWarlockProjectile::BeginPlay()
{
    Super::BeginPlay();Mesh->SetRelativeScale3D(FVector(Radius/50));SetMaterial(Mesh,TEXT("/Game/WarlockArena/Materials/M_Glow.M_Glow"),Kind==2?FLinearColor(.3f,1,1):FLinearColor(1,.22f,.015f));
    InitializeFireParticles();
}
void AWarlockProjectile::Step(float Dt)
{
    if(IsActorBeingDestroyed()||!Caster)return;
    Life-=Dt;if(Life<=0){Destroy();return;}
    auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();if(!M)return;
    FVector Start=GetActorLocation(),End=Start+Velocity*Dt;
    auto Distance=[&](FVector P){return FMath::PointDistToSegment(P,Start,End);};
    // Select the earliest collision along this step, rather than roster order.
    float Best=MAX_flt;AWarlockPawn* Hit=nullptr;int Pillar=-1;
    for(auto P:M->Fighters)if(P->bAlive&&P!=Caster&&P!=LastHit.Get()&&Caster->IsEnemy(P)&&Distance(P->GetActorLocation())<(P->ShieldRemaining>0?120:30)+Radius)
    {float T=FVector::DistSquared(Start,P->GetActorLocation());if(T<Best){Best=T;Hit=P;}}
    if(M->Arena)for(int I=0;I<M->Arena->Pillars.Num();++I)if(M->Arena->PillarHealth[I]>0&&Distance(M->Arena->Pillars[I]+FVector(0,0,40))<35+Radius)
    {float T=FVector::DistSquared2D(Start,M->Arena->Pillars[I]);if(T<Best){Best=T;Hit=nullptr;Pillar=I;}}
    if(Hit)
    {
        if(Hit->ShieldRemaining>0&&Bounces<8){Caster=Hit;Velocity=-Velocity;LastHit=Hit;++Bounces;SetActorLocation(Hit->GetActorLocation()+Velocity.GetSafeNormal()*65);ForceNetUpdate();return;}
        if(Kind==2){Caster->ChainTarget=Hit;Caster->ChainTime=MAX_flt;Caster->ChainDamageTimer=.18f;Caster->ChainSteps=0;M->Pulse(Hit->GetActorLocation(),60,1);}
        else Hit->Hit(Caster,Damage,Velocity,Push);
        Destroy();return;
    }
    if(Pillar>=0)
    {
        if(Kind==2){Caster->ChainTarget.Reset();Caster->ChainAnchor=M->Arena->Pillars[Pillar]+FVector(0,0,40);Caster->ChainTime=MAX_flt;Caster->ChainSteps=0;}
        else M->Arena->DamagePillar(Pillar,Damage,Caster);
        Destroy();return;
    }
    SetActorLocation(End);
}
void AWarlockProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(AWarlockProjectile,Caster);DOREPLIFETIME(AWarlockProjectile,Radius);DOREPLIFETIME(AWarlockProjectile,Kind);}
AWarlockPulse::AWarlockPulse()
{
    PrimaryActorTick.bCanEverTick=true;bReplicates=true;
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pulse"));SetRootComponent(Mesh);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetStaticMesh(Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
}
void AWarlockPulse::BeginPlay(){Super::BeginPlay();SetLifeSpan(Kind==2?1.15f:.35f);SetMaterial(Mesh,TEXT("/Game/WarlockArena/Materials/M_Glow.M_Glow"),Kind==1?FLinearColor(.15f,.65f,1):Kind==2?FLinearColor(.8f,.08f,.01f):FLinearColor(1,.35f,.03f));}
void AWarlockPulse::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;float Scale=Kind==2?1.f:FMath::Clamp(Age/.35f,.05f,1.f);Mesh->SetWorldScale3D(FVector(Size/50*Scale,Size/50*Scale,.008f));
}
void AWarlockPulse::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(AWarlockPulse,Size);DOREPLIFETIME(AWarlockPulse,Kind);}
AWarlockArena::AWarlockArena()
{
    PrimaryActorTick.bCanEverTick=true;bReplicates=true;
    Floor=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Platform"));SetRootComponent(Floor);Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Floor->SetStaticMesh(Shape(TEXT("/Game/WarlockArena/Environment/SM_ArenaPlatform.SM_ArenaPlatform")));Floor->SetRelativeLocation(FVector(0,0,-45));
}
void AWarlockArena::BeginPlay()
{
    Super::BeginPlay();SetMaterial(Floor,TEXT("/Game/WarlockArena/Materials/M_Stone.M_Stone"),FLinearColor(.11f,.105f,.095f));
    if(HasAuthority()||PillarHealth.IsEmpty())PillarHealth.Init(40,4);
    // Temporary authored layout; original terrain grid and obstacle variants remain CHECK05.
    for(int I=0;I<4;++I)
    {
        FVector P((I&1?1:-1)*896.f/1.5f,(I&2?1:-1)*896.f/1.5f,0);Pillars.Add(P);
        auto Mesh=NewObject<UStaticMeshComponent>(this);Mesh->SetupAttachment(Floor);Mesh->SetStaticMesh(Shape(TEXT("/Game/WarlockArena/Environment/SM_ArenaPillar.SM_ArenaPillar")));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->RegisterComponent();Mesh->SetWorldLocation(P+FVector(0,0,65));Mesh->SetWorldScale3D(FVector(.7f,.7f,1.3f));
        SetMaterial(Mesh,TEXT("/Game/WarlockArena/Materials/M_Stone.M_Stone"),FLinearColor(.08f,.075f,.068f));PillarMeshes.Add(Mesh);
    }
    if(GetNetMode()!=NM_DedicatedServer)if(auto Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WarlockArena/Materials/M_ArcaneSprite.M_ArcaneSprite")))for(int I=0;I<24;++I)
    {
        auto Sprite=NewObject<UMaterialBillboardComponent>(this);Sprite->SetupAttachment(Floor);Sprite->SetAbsolute(true,true,true);Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);Sprite->SetCastShadow(false);
        auto MID=UMaterialInstanceDynamic::Create(Base,this);MID->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.65f,.22f,.035f));MID->SetScalarParameterValue(TEXT("Opacity"),0);Sprite->AddElement(MID,nullptr,false,1.8f,3.2f,nullptr);Sprite->RegisterComponent();AmbientMotes.Add(Sprite);AmbientMaterials.Add(MID);
    }
}
void AWarlockArena::Tick(float Dt)
{
    Super::Tick(Dt);if(auto M=GetWorld()->GetGameState<AWarlockMatchState>())
    {
        AmbientAge+=Dt;
        for(int I=0;I<AmbientMotes.Num();++I){float Phase=FMath::Frac(AmbientAge*.075f+I*.217f),Angle=I*2*PI/24+AmbientAge*.018f,Radius=M->ArenaRadius+90+I%5*35;
            AmbientMotes[I]->SetWorldLocation(FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,15+Phase*210));AmbientMaterials[I]->SetScalarParameterValue(TEXT("Opacity"),.22f*FMath::Square(FMath::Sin(Phase*PI)));}
        for(int I=0;I<PillarMeshes.Num();++I)if(PillarMeshes[I].IsValid()&&PillarHealth.IsValidIndex(I))PillarMeshes[I]->SetVisibility(PillarHealth[I]>0);
        if(LastRadius!=M->ArenaRadius){LastRadius=M->ArenaRadius;Floor->SetWorldScale3D(FVector(LastRadius/50,LastRadius/50,.7f));
            for(int I=0;I<PillarMeshes.Num();++I)if(PillarMeshes[I].IsValid()){PillarMeshes[I]->SetWorldLocation(Pillars[I]+FVector(0,0,65));PillarMeshes[I]->SetWorldScale3D(FVector(.7f,.7f,1.3f));}}
    }
}
void AWarlockArena::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(AWarlockArena,PillarHealth);}
void AWarlockArena::DamagePillar(int I,float Damage,AWarlockPawn* Caster)
{
    if(!PillarHealth.IsValidIndex(I)||PillarHealth[I]<=0)return;PillarHealth[I]-=Damage;
    if(PillarHealth[I]<=0&&PillarMeshes[I].IsValid()){PillarMeshes[I]->SetVisibility(false);if(auto M=GetWorld()->GetAuthGameMode<AWarlockMode>())M->Pulse(Pillars[I],90,0);}
}
