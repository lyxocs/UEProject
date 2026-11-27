#include "WarlockGame.h"
#include "WarlockCatalog.h"
#include "WarlockSpellFX.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimSequence.h"

namespace
{
float Range(const AWarlockState* S){auto P=Cast<AWarlockPawn>(S->GetPawn());return 1+.12f*((P?P->RangeRank():S->RangeMastery)-S->Items[7]);}
float Time(const AWarlockPawn* P){auto S=P->State();return FMath::Max(.1f,1+.08f*P->TimeRank()-.05f*S->Items[0]-(P->DisruptRemaining>0?.3f:0));}
float Debuff(const AWarlockPawn* P){return Time(P)*(P->State()->Items[0]?.65f:1.f);}
AWarlockProjectile* Shot(AWarlockPawn* P,int F,FVector Origin,FVector D,float Speed,float Life,float Damage,float Push,int Kind=2)
{
    FActorSpawnParameters Params;Params.bDeferConstruction=true;
    auto A=P->GetWorld()->SpawnActor<AWarlockProjectile>(AWarlockProjectile::StaticClass(),Origin,D.Rotation(),Params);
    if(A){auto S=P->State();int Slot=WarlockCatalog::Slot(F);A->Caster=P;A->Family=F;A->SkillLevel=S->Level(Slot);A->bAlternate=S->Modes[Slot];A->Kind=Kind;A->Velocity=D.GetSafeNormal2D()*Speed;A->Life=A->InitialLife=Life;A->Damage=Damage;A->Push=Push;A->Radius=F==WarlockCatalog::Boomerang?38:F==WarlockCatalog::Seeker?29:F==WarlockCatalog::Swap?40:F==WarlockCatalog::Drain?(A->bAlternate?35:24.5f):F==WarlockCatalog::Bounce?(A->bAlternate?35:38):F==WarlockCatalog::Control?52:F==WarlockCatalog::Split?20:25;A->TravelTarget=P->AimPoint;A->TravelStart=Origin;A->FinishSpawning(FTransform(Origin));}
    return A;
}
}
int AWarlockPawn::LifeRank() const{return State()?State()->LifeMastery+(EmpowerRemaining>0?EmpowerLife:0):0;}
int AWarlockPawn::RangeRank() const{return State()?State()->RangeMastery+(EmpowerRemaining>0?EmpowerRange:0):0;}
int AWarlockPawn::TimeRank() const{return State()?State()->TimeMastery+(EmpowerRemaining>0?EmpowerTime:0):0;}
void AWarlockPawn::AddSpellEffect(int F,AWarlockPawn* Source,float Life,float Speed,float Output,float Push)
{
    if(!HasAuthority()||!bAlive||!Source||!FMath::IsFinite(Life))return;
    for(auto& E:SpellEffects)if(F!=WarlockCatalog::Drain&&E.Family==F&&E.Source.Get()==Source){E={F,Source,Life,Speed,Output,Push};return;}
    SpellEffects.Add({F,Source,Life,Speed,Output,Push});
}
void AWarlockPawn::ReturnRewind()
{
    if(RewindRemaining<=0)return;RewindRemaining=0;CancelCast();
    AWarlockSpellFX::Spawn(GetWorld(),2,GetActorLocation(),RewindPosition,80,this,6);
    SetActorLocation(RewindPosition);SetActorRotation(RewindFacing);Momentum=RewindMomentum;DashRemaining=RewindDash;
    Health=FMath::Max(Health,RewindHealth);
    Knockback=RewindKnockback+FMath::Max(0.f,Knockback-RewindKnockback)*FMath::Max(0.f,.8f-.1f*State()->Level(6));
    ChainTime=0;ChainTarget.Reset();bClickMoving=false;++BlinkSerial;ForceNetUpdate();
}
void AWarlockPawn::FormMirrors(FVector Target)
{
    if(!HasAuthority()||MirrorPending<=0)return;MirrorPending=0;MirrorRemaining=4*Time(this);
    FVector O=GetActorLocation(),D=(Target-O).GetSafeNormal2D();if(D.IsNearlyZero())D=GetActorForwardVector();
    SetActorLocation(O+D*167);++BlinkSerial;
    for(float Angle:{120.f,-120.f})
    {
        FActorSpawnParameters Params;Params.bDeferConstruction=true;
        auto Image=GetWorld()->SpawnActor<AWarlockImage>(AWarlockImage::StaticClass(),O,GetActorRotation(),Params);
        if(Image){Image->Source=this;Image->Offset=D.RotateAngleAxis(Angle,FVector::UpVector)*167-D*167;Image->FinishSpawning(FTransform(O+Image->Offset));Image->SetLifeSpan(MirrorRemaining);}
    }
    AWarlockSpellFX::Spawn(GetWorld(),2,O,GetActorLocation(),100,this,6);ForceNetUpdate();
}
AWarlockImage::AWarlockImage()
{
    bReplicates=true;PrimaryActorTick.bCanEverTick=true;
    Visual=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mirror"));SetRootComponent(Visual);Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);Visual->SetCastShadow(false);
}
void AWarlockImage::Tick(float Dt)
{
    Super::Tick(Dt);if(!Source)return;
    if(HasAuthority()&&(!Source->bAlive||Source->MirrorRemaining<=0)){Destroy();return;}
    SetActorLocation(Source->GetActorLocation()+Offset+FVector(0,0,-40));SetActorRotation(Source->GetActorRotation());
    if(GetNetMode()==NM_DedicatedServer)return;
    if(!Visual->GetSkeletalMeshAsset()&&Source->Avatar->GetSkeletalMeshAsset()){Visual->SetSkeletalMeshAsset(Source->Avatar->GetSkeletalMeshAsset());Visual->SetRelativeScale3D(Source->AvatarRestScale);for(int I=0;I<Source->Avatar->GetNumMaterials();++I)Visual->SetMaterial(I,Source->Avatar->GetMaterial(I));Visual->PlayAnimation(Source->IdleAnimation,true);}
    if(SeenCast!=Source->CastSerial){SeenCast=Source->CastSerial;if(Source->LastCastSlot<8)Visual->PlayAnimation(Source->AttackAnimation,false);}
    else if(!Visual->IsPlaying())Visual->PlayAnimation(Source->LocomotionSpeed>10?Source->WalkAnimation:Source->IdleAnimation,true);
}
void AWarlockImage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(AWarlockImage,Source);DOREPLIFETIME(AWarlockImage,Offset);}
void AWarlockPawn::StepSpellEffects(float Dt)
{
    SpellSpeed=0;SpellOutput=SpellPush=1;
    EmpowerRemaining=FMath::Max(0.f,EmpowerRemaining-Dt);
    for(auto& E:SpellEffects){E.Remaining-=Dt;if(E.Remaining>0&&E.Source.IsValid()){SpellSpeed+=E.Speed;SpellOutput*=E.Output;SpellPush*=E.Push;}}
    SpellEffects.RemoveAll([](const FTimedSpell& E){return E.Remaining<=0||!E.Source.IsValid();});
    for(auto& S:SilenceRemaining)S=FMath::Max(0.f,S-Dt);
    RootRemaining=FMath::Max(0.f,RootRemaining-Dt);DisruptRemaining=FMath::Max(0.f,DisruptRemaining-Dt);
    StealthRemaining=FMath::Max(0.f,StealthRemaining-Dt);MirrorRemaining=FMath::Max(0.f,MirrorRemaining-Dt);MirrorPending=FMath::Max(0.f,MirrorPending-Dt);MatrixRemaining=FMath::Max(0.f,MatrixRemaining-Dt);
    if(MirrorPending>0&&!MoveInput.IsNearlyZero())FormMirrors(GetActorLocation()+MoveInput*200);
    if(RewindRemaining>0){if(RewindRemaining<=Dt)ReturnRewind();else RewindRemaining-=Dt;}
    if(DashRemaining>0)
    {
        DashRemaining=FMath::Max(0.f,DashRemaining-Dt);
        if(!bWindDash)Momentum=DashDirection*39;
        else {FVector Input=bClickMoving?(ClickTarget-GetActorLocation()).GetSafeNormal2D():MoveInput.GetSafeNormal2D();if(!Input.IsNearlyZero()&&FVector::DotProduct(Input,DashDirection)<.99f){Momentum+=Input*20;DashDirection=Input;}}
        auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();
        if(M&&!bWindDash)for(auto P:M->Fighters)if(P->bAlive&&IsEnemy(P)&&FVector::Dist2D(GetActorLocation(),P->GetActorLocation())<65)
        {float Ratio=FMath::Min(1.f,Momentum.SizeSquared2D()/(39*39*.8f));bool ShadowStrike=StealthRemaining>0&&!State()->Modes[5];float Damage=(5+.4f*State()->Level(3))*Ratio+(ShadowStrike?4.8f+.6f*State()->Level(5):0);P->Hit(this,Damage,DashDirection,ShadowStrike?.7f:1.f,3);if(ShadowStrike)StealthRemaining=0;else if(StealthRemaining>0)AddSpellEffect(WarlockCatalog::WindWalk,P,.5f);DashRemaining=0;Momentum*=.2f;break;}
    }
    if(StealthRemaining>0)
    {
        if(auto M=GetWorld()->GetAuthGameMode<AWarlockMode>())for(auto P:M->Fighters)if(P->bAlive&&IsEnemy(P)&&FVector::Dist2D(GetActorLocation(),P->GetActorLocation())<65&&!SpellEffects.ContainsByPredicate([P](const FTimedSpell& E){return E.Family==WarlockCatalog::WindWalk&&E.Source.Get()==P&&E.Remaining>0;}))
        {
            FVector D=(P->GetActorLocation()-GetActorLocation()).GetSafeNormal2D();
            if(!State()->Modes[5]){P->Hit(this,4.8f+.6f*State()->Level(5),D,1,5);StealthRemaining=0;}
            else
            {
                float Before=P->Health;P->Hit(this,.5f+.1f*State()->Level(5),FVector::ZeroVector,0,5);
                static const float Ring[]={0,.06f,.1f,.1f};auto TS=P->State();float Leech=1+(TS->Items[1]?.05f:0)+Ring[TS->Items[5]]+.05f*TS->Items[6]+(P->DisruptRemaining>0?.3f:0);
                float Heal=FMath::Max(0.f,Before-P->Health)*Leech;Health=FMath::Min(MaxHealth(),Health+Heal);Knockback=FMath::Max(0.f,Knockback-Heal*.5f);
                float Protect=(Protection>0?.1f:1.f)*(P->Protection>0?.1f:1.f);
                P->Momentum+=D*WarlockRules::Impulse(2.5f,P->Knockback,P->KnockbackScale()*SpellOutput*75/(100+Knockback)*Protect);
                Momentum-=D*WarlockRules::Impulse(2.5f,Knockback,KnockbackScale()*P->SpellOutput*100/(100+P->Knockback)*Protect);
                AddSpellEffect(WarlockCatalog::WindWalk,P,.5f);
            }
            break;
        }
    }
}
bool AWarlockPawn::ReleaseFamily(int Slot)
{
    using namespace WarlockCatalog;
    auto S=State();int F=Family(S,Slot),L=S->Level(Slot);bool Alt=S->Modes[Slot];auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();
    FVector O=CastOrigin(),D=(AimPoint-O).GetSafeNormal2D();if(D.IsNearlyZero())D=GetActorForwardVector();
    if(F==Fireball||F==Smite||F==Lightning||F==Blink||F==Spray||F==Reflect||F==Haste||F==Chain||(F==Meteor&&!Alt))return false;
    float R=Range(S),T=Time(this);
    switch(F)
    {
    case Dash:
        DashDirection=D;bWindDash=Alt;DashRemaining=Alt?(RootRemaining>0?1.f:2.7f)*T:FMath::Min(float(FVector::Dist2D(O,AimPoint)),900*R)/1300.f+.09f;
        Momentum=D*(Alt?20.f:30.03f);AWarlockSpellFX::Spawn(GetWorld(),9,O,O,100,this,3);break;
    case WindWalk:StealthRemaining=(Alt?4.f:2.6f)*T;AWarlockSpellFX::Spawn(GetWorld(),2,O,O,80,this,5);break;
    case Rewind:
        RewindPosition=O;RewindMomentum=Momentum;RewindFacing=GetActorRotation();RewindHealth=Health;RewindKnockback=Knockback;RewindDash=DashRemaining;RewindRemaining=3.5f*T;
        AWarlockSpellFX::Spawn(GetWorld(),5,O,O,100,this,6);break;
    case Mirror:MirrorPending=4;AWarlockSpellFX::Spawn(GetWorld(),5,O,O,100,this,6);break;
    case Control:
        if(Alt){MatrixPosition=O;MatrixRemaining=120;AWarlockSpellFX::Spawn(GetWorld(),5,O,O,100,this,7);}
        else Shot(this,F,O,D,800,3.3f*R,0,0);
        break;
    case WarlockCatalog::Swap:
        if(Alt)
        {
            AWarlockProjectile* Target=nullptr;float Best=84100;
            if(FVector::DistSquared2D(O,AimPoint)<2250000)for(TActorIterator<AWarlockProjectile> It(GetWorld());It;++It)
            {float Dist=FVector::DistSquared2D(It->GetActorLocation(),AimPoint);if(Dist<Best){Best=Dist;Target=*It;}}
            if(Target){FVector End=Target->GetActorLocation();Target->SetActorLocation(O);SetActorLocation(End);++BlinkSerial;AddSpellEffect(F,this,(Target->Caster==this?3:4)*T,100);AWarlockSpellFX::Spawn(GetWorld(),2,O,End,80,this,3);}
            else Cooldowns[Slot]=0;
        }
        else Shot(this,F,O,D,1700,FMath::Min(float(FVector::Dist2D(O,AimPoint)),800*R)/1700.f,0,0);
        break;
    case Boomerang:Shot(this,F,O,D,1500,800*R/1500.f,6.3f+.7f*L,.95f);break;
    case Seeker:Shot(this,F,O+D*65,D,800,4.5f*R,6.f+L,.95f);break;
    case Drain:Shot(this,F,O+(Alt?D*67:FVector::ZeroVector),D,Alt?850:700,Alt?R:FMath::Min(float(FVector::Dist2D(O,AimPoint)),700*R)/700.f,Alt?4.f+L:5.f+L,Alt?.6f:.2f);break;
    case Bounce:Shot(this,F,O,D,900,900*R/900.f,Alt?5.3f+.7f*L:4.6f+.8f*L,Alt?1.15f:1.2f);break;
    case Meteor:{auto A=Shot(this,F,O,D,400,1.43f*R,0,0,0);if(A)A->Radius=72+2*L;break;}
    case Split:{float Speed=Alt?280.f:700.f;float Dist=Alt?700*R:FMath::Min(float(FVector::Dist2D(O,AimPoint)),700*R);auto A=Shot(this,F,O,D,Speed,Dist/Speed,0,0);if(A){A->Radius=50;A->bField=Alt;}break;}
    case Blast:{auto A=Shot(this,F,O+(AimPoint-O).GetClampedToMaxSize2D(100),D,0,2.8f*T,0,0);if(A)A->bField=true;break;}
    case Bind:
        if(!Alt){auto A=Shot(this,F,O,D,1000,1.1f*R,0,0);if(A)A->Radius=39;}
        else
        {
            // df/Bf/If: the two travelling endpoints define a widening strip;
            // each slot's silence depends on the difference in learned levels.
            auto A=Shot(this,F,O-D*210,D.RotateAngleAxis(17.1887f,FVector::UpVector),900,(FMath::Min(float(FVector::Dist2D(O,AimPoint)),900*R)+210)/900.f*1.046751601f,0,0);if(A)A->Radius=23;
        }break;
    case Gravity:
        {auto A=Shot(this,F,O,D,Alt?850:400,Alt?FMath::Min(float(FVector::Dist2D(O,AimPoint)),850*R)/850.f:900*R/400.f,0,0);if(A)A->Radius=Alt?25:45;break;}
    default:return false;
    }
    ForceNetUpdate();return true;
}
void AWarlockProjectile::PrepareSpellStep(float Dt)
{
    using namespace WarlockCatalog;
    if(Family<0||!Caster||!Caster->State())return;
    SpellAge+=Dt;SpellTimer-=Dt;for(auto& Pair:ContactCooldown)Pair.Value=FMath::Max(0.f,Pair.Value-Dt);
    auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();if(!M)return;FVector O=GetActorLocation();int L=SkillLevel;
    if((Family==Boomerang&&bReturning)||Family==Seeker)
    {
        AWarlockPawn* Target=bReturning?Caster.Get():nullptr;float Best=MAX_flt;
        if(!Target)for(auto P:M->Fighters)if(P->bAlive&&Caster->IsEnemy(P)){float D=FVector::DistSquared2D(P->GetActorLocation(),TravelTarget);if(D<Best){Best=D;Target=P;}}
        if(Target){float Speed=Velocity.Size2D();FVector Desired=(Target->GetActorLocation()-O).GetSafeNormal2D()*Speed;Velocity=FMath::VInterpConstantTo(Velocity,Desired,Dt,Family==Boomerang?30.f:900.f);}
        if(bReturning&&FVector::Dist2D(O,Caster->GetActorLocation())<75){Destroy();return;}
        if(Family==Seeker)Damage=6.f+L+2.5f*FMath::Max(0.f,4.5f*(1+1.5f*.12f*(Caster->RangeRank()-Caster->State()->Items[7]))-2.25f-Life)/2.25f;
    }
    if(Family==Drain&&bReturning)
    {
        FVector D=Caster->GetActorLocation()-O;
        if(D.Size2D()<40){Caster->Health=FMath::Min(Caster->MaxHealth(),Caster->Health+Damage);Caster->Knockback=FMath::Max(0.f,Caster->Knockback-.5f*Damage);Caster->ForceNetUpdate();Destroy();return;}
        Velocity=D.GetSafeNormal2D()*1000;
    }
    if(Family==Bounce&&bReturning)
    {
        FVector D=Caster->GetActorLocation()-O;
        if(D.Size2D()<64){Caster->Cooldowns[4]=0;Caster->ForceNetUpdate();Destroy();return;}
        Velocity=D.GetSafeNormal2D()*400;
    }
    if(Family==Blast&&SpellTimer<=0)
    {
        SpellTimer+=.18f;FVector D=(O-Caster->GetActorLocation()).GetSafeNormal2D();if(D.IsNearlyZero())D=(TravelTarget-O).GetSafeNormal2D();
        auto A=Shot(Caster,Split,O,D,1000,.9f*Range(Caster->State()),2.7f+.3f*L,.6f,1);if(A){A->bAlternate=true;A->Strength=-1;A->SkillLevel=L;}
    }
    if(Family==Split&&Strength>=0&&SpellTimer<=0&&(bAlternate||bField))
    {
        SpellTimer+=bAlternate?.18f:.12f;
        FVector D=Velocity.GetSafeNormal2D();if(D.IsNearlyZero())D=FVector(1,0,0);
        for(float Sign:{-1.f,1.f})
        {
            FVector V=bAlternate?D.RotateAngleAxis(Bounces*30.f+(Sign<0?180.f:0.f),FVector::UpVector)*750:(D+D.RotateAngleAxis(90,FVector::UpVector)*Life*Sign)/(1+Life)*600;
            auto A=Shot(Caster,Split,O,V,V.Size2D(),bAlternate?1.f:1.2f*Range(Caster->State()),2.5f+.5f*L,.65f,1);if(A){A->Strength=-1;A->SkillLevel=L;A->bAlternate=false;}
        }
        ++Bounces;
    }
    if(Family==Gravity&&!bAlternate)
    {
        for(auto P:M->Fighters)if(P->bAlive&&Caster->IsEnemy(P))
        {
            float D=FVector::DistSquared2D(O,P->GetActorLocation());
            float Mass=1+.1f*P->State()->Items[7]-(P->State()->Items[4]>.0f?.05f:0)-(P->DashRemaining>0&&P->bWindDash?.6f:0);
            if(D<202500)P->Momentum+=(O-P->GetActorLocation()).GetSafeNormal2D()*(.1f*L+1.5f)*(1-D/250000.f)/(FMath::Max(.1f,Mass)+1)*Dt/.03f;
            if(SpellTimer<=0&&D<62500)P->Hit(Caster,.1f+.2f*L,O-P->GetActorLocation(),0,7);
        }
        for(TActorIterator<AWarlockProjectile> It(GetWorld());It;++It)if(*It!=this&&It->Family!=Gravity){float D=FVector::DistSquared2D(O,It->GetActorLocation());if(D<390625)It->Velocity+=(O-It->GetActorLocation()).GetSafeNormal2D()*50*(1-D/390625)*Dt/.03f;}
        if(SpellTimer<=0)SpellTimer+=.18f;
    }
    if(Family==Gravity&&bField&&SpellTimer<=0)
    {
        SpellTimer+=.18f;
        for(auto P:M->Fighters)if(P->bAlive&&FVector::Dist2D(O,P->GetActorLocation())<200)
        {if(!Caster->IsEnemy(P))P->Health=FMath::Min(P->MaxHealth(),P->Health+P->MaxHealth()*(.0012f+.0003f*L));else {P->AddSpellEffect(Gravity,Caster,.3f,-P->MoveSpeed()*.45f);P->Hit(Caster,.15f+.25f*L,FVector::ZeroVector,0,7);}}
    }
    if(Family==Meteor)
    {
        if(SpellTimer<=0)
        {
            SpellTimer+=.18f;
            for(auto P:M->Fighters)if(P->bAlive&&Caster->IsEnemy(P)&&FVector::Dist2D(O,P->GetActorLocation())<Radius+30)
            {P->Hit(Caster,.5f+.3f*L,Velocity,0,5);P->AddSpellEffect(Meteor,Caster,.25f,-P->MoveSpeed()*.5f);P->Momentum+=Velocity.GetSafeNormal2D()*FMath::Max(0.f,12.75f-FVector::DotProduct(Velocity.GetSafeNormal2D(),P->Momentum));}
        }
    }
    if(Family==Bind&&bAlternate)
    {
        FVector Axis=(TravelTarget-TravelStart).GetSafeNormal2D();FVector Side=Axis.RotateAngleAxis(90,FVector::UpVector);float Along=SpellAge*900;
        FVector Center=TravelStart+Axis*Along;float Half=Along*FMath::Sin(.3f);
        for(auto P:M->Fighters)if(P->bAlive&&Caster->IsEnemy(P)&&FMath::Abs(float(FVector::DotProduct(P->GetActorLocation()-Center,Axis)))<100&&FMath::Abs(float(FVector::DotProduct(P->GetActorLocation()-Center,Side)))<Half)
        {
            if(ContactCooldown.Contains(P))continue;ContactCooldown.Add(P,1000);
            if(P->SilenceRemaining.Num()!=8)P->SilenceRemaining.Init(0,8);
            float Credit=0;for(int I=0;I<8;++I)if(I!=1&&P->State()->Level(I)>0){int Difference=L-P->State()->Level(I);if(Difference>=-2){float Last=(I==3?.5f:1.f)*(4+Difference)*Time(Caster)*Debuff(P);P->SilenceRemaining[I]=FMath::Max(P->SilenceRemaining[I],Last);Credit+=Last;}}
            P->Contributions.FindOrAdd(Caster)+=Credit/3;P->LastAttacker=Caster;
            P->CancelCast();P->ForceNetUpdate();
        }
    }
}
bool AWarlockProjectile::SpellContact(AWarlockPawn* Target,int Pillar,FVector Position)
{
    using namespace WarlockCatalog;
    if(Family<0)return false;auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();if(!M||!Caster)return true;
    int L=SkillLevel,Slot=WarlockCatalog::Slot(Family);SetActorLocation(Position);
    if(Family==Meteor){if(Pillar>=0){M->Arena->DamagePillar(Pillar,10+2*L,Caster,5);EndSpell();Destroy();}return true;}
    if(Family==Gravity||Family==Blast||(Family==Bind&&bAlternate)||(Family==Split&&bAlternate&&Strength>=0))return true;
    if(Family==WarlockCatalog::Swap)
    {
        FVector Origin=Caster->GetActorLocation(),End=Target?Target->GetActorLocation():Pillar>=0?M->Arena->Pillars[Pillar]+FVector(0,0,40):Position;
        // The original rejects a safe-ground ally swap into lava.
        if(Target&&!Caster->IsEnemy(Target)&&Origin.Size2D()>M->Match()->ArenaRadius&&End.Size2D()<=M->Match()->ArenaRadius){Destroy();return true;}
        Caster->SetActorLocation(End);++Caster->BlinkSerial;if(Target){if(Caster->IsEnemy(Target))Target->LastAttacker=Caster;Target->SetActorLocation(Origin);++Target->BlinkSerial;Target->ForceNetUpdate();}
        else if(Pillar>=0){M->Arena->Pillars[Pillar]=FVector(Origin.X,Origin.Y,0);M->Arena->ForceNetUpdate();}
        AWarlockSpellFX::Spawn(GetWorld(),2,Origin,End,80,Caster,3);Caster->ForceNetUpdate();Destroy();return true;
    }
    if(Family==Bind&&Target){Target->RootRemaining=(4+.25f*L)*Time(Caster)*Debuff(Target);Target->Contributions.FindOrAdd(Caster)+=Target->RootRemaining*2;Target->LastAttacker=Caster;Target->CancelCast();Target->bClickMoving=false;Target->ForceNetUpdate();Destroy();return true;}
    if(Family==Control&&Target){Target->DisruptRemaining=FMath::Max(0.f,Life-.15f);Target->Contributions.FindOrAdd(Caster)+=Target->DisruptRemaining*2;Target->LastAttacker=Caster;Target->DisruptProjectile=this;Target->ForceNetUpdate();ContactCooldown.Add(Target,Life);Velocity=Velocity.GetSafeNormal2D()*400;return true;}
    if(Family==Drain&&Target)
    {
        if(Caster->IsEnemy(Target))
        {
            float Before=Target->Health;Target->Hit(Caster,Damage,Velocity,Push,4);
            Target->AddSpellEffect(Drain,Caster,(bAlternate?7.5f:3.f+L)*Time(Caster)*Debuff(Target),bAlternate?0:-50,bAlternate?.5f:1.f);
            float Heal=FMath::Max(0.f,Before-Target->Health)*(1+.1f*Caster->LifeRank());
            auto Return=Shot(Caster,Drain,Position,-Velocity,1000,5,Heal,0);if(Return){Return->bReturning=true;Return->bField=true;}
        }
        else
        {
            Target->Health=FMath::Min(Target->MaxHealth(),Target->Health+Damage);Target->AddSpellEffect(Drain,Caster,(bAlternate?7.5f:3.f+L)*Time(Caster)*Time(Target),bAlternate?0:50,bAlternate?1.1f:1.f);
            if(bAlternate){AWarlockPawn* Next=nullptr;float Best=MAX_flt;for(auto P:M->Fighters)if(P->bAlive&&Caster->IsEnemy(P)){float D=FVector::DistSquared2D(Position,P->GetActorLocation());if(D<Best){Best=D;Next=P;}}if(Next){Strength=2;LastHit=Target;Velocity=(Next->GetActorLocation()-Position).GetSafeNormal2D()*550*.85f;Life=Range(Caster->State());return true;}}
        }
        Destroy();return true;
    }
    if(Family==Split&&Strength>=0)
    {
        if(Target)Target->Hit(Caster,3,Velocity,1.4f,5);
        EndSpell();Destroy();return true;
    }
    float Amount=Family==Boomerang?Damage*(1+.1f*Bounces):Family==Bounce?Damage*(bAlternate?1+.05f*Caster->BounceCharge:Strength):Damage;
    float Force=Family==Boomerang?Push/(1+.1f*Bounces):Push;
    if(Target)Target->Hit(Caster,Amount,Velocity,Force,Slot);else if(Pillar>=0)M->Arena->DamagePillar(Pillar,Amount,Caster,Slot);
    if(Family==Boomerang||Family==Bounce)
    {
        if(Family==Bounce&&bAlternate)
        {
            if(Target){++Caster->BounceCharge;auto Orb=Shot(Caster,Bounce,Position,-Velocity,400,10,0,0);if(Orb){Orb->bReturning=true;Orb->bField=true;}}
            Destroy();return true;
        }
        ++Bounces;if(Target)ContactCooldown.Add(Target,.25f);
        if(Family==Boomerang){Velocity=-Velocity.GetSafeNormal2D()*500;bReturning=true;Life=FMath::Max(Life,3.f);}
        else
        {
            Strength=FMath::Max(.2f,Strength*.7f);
            AWarlockPawn* Next=nullptr;float Best=MAX_flt;for(auto P:M->Fighters)if(P->bAlive&&P!=Target&&Caster->IsEnemy(P)){float D=FVector::DistSquared2D(Position,P->GetActorLocation());if(D<Best){Best=D;Next=P;}}
            Velocity=Next?(Next->GetActorLocation()-Position).GetSafeNormal2D()*900:-Velocity;Life=(750+150*L)*Range(Caster->State())/900.f;
        }
        if(Bounces>=32)Destroy();return true;
    }
    Destroy();return true;
}
void AWarlockProjectile::EndSpell()
{
    using namespace WarlockCatalog;
    if(!Caster||!Caster->State())return;auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();if(!M)return;
    if(Family==Boomerang&&!bReturning){bReturning=true;Velocity=-Velocity.GetSafeNormal2D()*500;Life=3;return;}
    if(Family==Bounce&&bAlternate&&!bReturning)Caster->BounceCharge=0;
    if(Family==WarlockCatalog::Swap){SpellContact(nullptr,-1,GetActorLocation());return;}
    if(Family==Gravity&&bAlternate&&!bField){bField=true;Velocity=FVector::ZeroVector;Life=5*Time(Caster);Radius=200;AWarlockSpellFX::Spawn(GetWorld(),5,GetActorLocation(),GetActorLocation(),200,Caster,7);return;}
    if(Family==Split&&Strength>=0&&!bAlternate&&!bField)
    {
        auto Emitter=Shot(Caster,Split,GetActorLocation(),Velocity,250,.48f,0,0);if(Emitter){Emitter->bField=true;Emitter->bAlternate=false;Emitter->SkillLevel=SkillLevel;}
        AWarlockSpellFX::Spawn(GetWorld(),0,GetActorLocation(),GetActorLocation(),120,Caster,5);
    }
    if(Family==Meteor)
    {
        float Size=1.5f*FMath::Sqrt(128+2*Radius)/14.142f*200;
        AWarlockSpellFX::Spawn(GetWorld(),0,GetActorLocation(),GetActorLocation(),Size,Caster,5);
        for(auto P:M->Fighters)if(P->bAlive&&Caster->IsEnemy(P)&&FVector::Dist2D(GetActorLocation(),P->GetActorLocation())<Size)P->Hit(Caster,3+1.5f*SkillLevel,P->GetActorLocation()-GetActorLocation(),.92f,5);
    }
}
