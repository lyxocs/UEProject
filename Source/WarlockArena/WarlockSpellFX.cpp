#include "WarlockSpellFX.h"
#include "WarlockGame.h"
#include "Scalability.h"
#include "WarlockTuning.h"
#include "WarlockAudio.h"
#include "WarlockCatalog.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

namespace
{
FLinearColor SpellColor(int Slot)
{
    const FLinearColor Colors[]={FLinearColor(1,.19f,.012f),FLinearColor(1,.58f,.13f),FLinearColor(.16f,.6f,1),FLinearColor(.43f,.28f,1),FLinearColor(1,.22f,.025f),FLinearColor(1,.11f,.02f),FLinearColor(.12f,.68f,.85f),FLinearColor(.25f,.5f,1)};
    return Colors[FMath::Clamp(Slot,0,7)];
}
}
AWarlockSpellFX::AWarlockSpellFX()
{
    PrimaryActorTick.bCanEverTick=true;bReplicates=true;SetReplicateMovement(false);
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("EffectRoot")));
}
AWarlockSpellFX* AWarlockSpellFX::Spawn(UWorld* World,int InStyle,FVector Position,FVector InEnd,float InSize,AWarlockPawn* InSource,int InSlot)
{
    if(auto M=World->GetAuthGameMode<AWarlockMode>();M&&M->bTraining)return nullptr;
    FActorSpawnParameters Params;Params.bDeferConstruction=true;
    auto FX=World->SpawnActor<AWarlockSpellFX>(StaticClass(),Position,FRotator::ZeroRotator,Params);
    if(FX){FX->Style=InStyle;FX->End=InEnd;FX->Size=InSize;FX->Source=InSource;FX->Slot=InSlot;FX->SourceCastSerial=InSource?InSource->CastSerial:0;FX->FinishSpawning(FTransform(Position));}
    return FX;
}
void AWarlockSpellFX::BeginPlay()
{
    Super::BeginPlay();
    if(Style>=11&&Style<=13){Duration=.12f;UWarlockAudio::Event(GetWorld(),Slot,Style==11?0:Style==12?2:3,GetActorLocation());SetLifeSpan(.2f);return;}
    if(Style==8&&Source)UWarlockAudio::Event(GetWorld(),Slot,1,GetActorLocation());
    Duration=Style==4?1.15f:Style==6?30.f:Style==7?(Source?Source->CastDuration:.7f)+.2f:Style==1?.3f:Style==2?.24f:.65f;
    if(Style==9)Duration=Source?FMath::Max(.3f,Source->HasteRemaining):6.f;
    if(Style==7)Color=SpellColor(Slot);else if(Style==1||Style==2||Style==5||Style==6)Color=SpellColor(Style==1?2:Style==2?3:Style==5?6:7);else Color=SpellColor(Style==0?1:5);
    if(Style==9||(Style==7&&Slot==6&&Source&&Source->State()&&Source->State()->DefenseChoice==1))Color=FLinearColor(.25f,.85f,.55f);
    if(Style==8||Style==0)Color=SpellColor(Slot);
    if(GetNetMode()==NM_DedicatedServer){SetLifeSpan(Duration+1);return;}
    auto SpriteBase=LoadObject<UMaterialInterface>(nullptr,Style==4?TEXT("/Game/WarlockArena/Materials/M_FlameTongue.M_FlameTongue"):TEXT("/Game/WarlockArena/Materials/M_ArcaneSprite.M_ArcaneSprite"));
    auto RingBase=LoadObject<UMaterialInterface>(nullptr,Style==4?TEXT("/Game/WarlockArena/Materials/M_MeteorWarning.M_MeteorWarning"):Style==0||Style==8?TEXT("/Game/WarlockArena/Materials/M_ShockwaveRing.M_ShockwaveRing"):TEXT("/Game/WarlockArena/Materials/M_ArcaneRing.M_ArcaneRing"));
    auto BeamBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WarlockArena/Materials/M_Glow.M_Glow"));
    auto Plane=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
    auto Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if(RingBase&&Style!=1&&Style!=6&&Style!=2)
    {
        Ring=NewObject<UStaticMeshComponent>(this);Ring->SetupAttachment(RootComponent);Ring->SetStaticMesh(Plane);Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);Ring->SetCastShadow(false);Ring->RegisterComponent();
        auto MID=UMaterialInstanceDynamic::Create(RingBase,this);MID->SetVectorParameterValue(TEXT("Tint"),Color);Ring->SetMaterial(0,MID);Materials.Add(MID);
        Ring->SetWorldLocation(GetActorLocation()+FVector(0,0,-35));
    }
    const int BaseCount=Style==1?18:Style==6?0:Style==7?12:Style==0?48:Style==4?24:32;
    int Quality=Scalability::GetQualityLevels().EffectsQuality;
    const int Count=BaseCount==0?0:Quality>=2?BaseCount:FMath::Max(6,FMath::RoundToInt(BaseCount*(Quality==0?.33f:.6f)));
    if(SpriteBase)for(int I=0;I<Count;++I)
    {
        float A=2*PI*I/FMath::Max(1,Count);FVector Radial(FMath::Cos(A),FMath::Sin(A),0);
        FParticle State;State.Life=Duration*FMath::FRandRange(.55f,1.f);State.Size=I%4==0?2.5f:6.f;State.Position=GetActorLocation();
        if(Style==2)State.Position=(I%2?End:GetActorLocation())+FVector(FMath::Cos(A)*50,FMath::Sin(A)*50,35);
        if(Style==1)State.Position=FMath::Lerp(GetActorLocation(),End,float(I)/Count);
        State.Velocity=Radial*FMath::FRandRange(60.f,Size*2)+FVector(0,0,FMath::FRandRange(8.f,80.f));
        if(Style==8)State.Velocity+=(End-GetActorLocation()).GetSafeNormal2D()*130;
        if(Style==1||Style==2)State.Velocity=Radial*30+FVector(0,0,25);
        auto Sprite=NewObject<UMaterialBillboardComponent>(this);Sprite->SetupAttachment(RootComponent);Sprite->SetAbsolute(true,true,true);Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);Sprite->SetCastShadow(false);
        auto MID=UMaterialInstanceDynamic::Create(SpriteBase,this);MID->SetVectorParameterValue(TEXT("Tint"),Color);MID->SetScalarParameterValue(TEXT("Opacity"),.7f);Sprite->AddElement(MID,nullptr,false,State.Size,State.Size*1.8f,nullptr);Sprite->RegisterComponent();
        Particles.Add(Sprite);Materials.Add(MID);States.Add(State);
    }
    if(Style==1||Style==6)for(int I=0;I<(Style==1?36:20);++I)
    {
        auto Beam=NewObject<UStaticMeshComponent>(this);Beam->SetupAttachment(RootComponent);Beam->SetStaticMesh(Cylinder);Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);Beam->SetCastShadow(false);Beam->RegisterComponent();
        Beam->SetWorldScale3D(FVector(.001f));
        auto MID=UMaterialInstanceDynamic::Create(BeamBase,this);MID->SetVectorParameterValue(TEXT("Tint"),Color*(I%2?.55f:1.2f));Beam->SetMaterial(0,MID);Beams.Add(Beam);Materials.Add(MID);
    }
    if(Style==2||(Style==7&&Slot==3&&Source&&WarlockCatalog::Family(Source->State(),3)==WarlockCatalog::Blink))
    {
        auto PortalBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WarlockArena/Materials/M_ApparitionPortal.M_ApparitionPortal"));
        if(PortalBase)for(int I=0;I<(Style==2?2:1);++I)
        {
            auto Portal=NewObject<UMaterialBillboardComponent>(this);Portal->SetupAttachment(RootComponent);Portal->SetAbsolute(true,true,true);Portal->SetCollisionEnabled(ECollisionEnabled::NoCollision);Portal->SetCastShadow(false);
            auto MID=UMaterialInstanceDynamic::Create(PortalBase,this);MID->SetScalarParameterValue(TEXT("Opacity"),1);MID->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.10f,.08f,.16f));
            Portal->AddElement(MID,nullptr,false,48,78,nullptr);Portal->RegisterComponent();Portal->SetWorldLocation((I?End:GetActorLocation())+FVector(0,0,40));Portals.Add(Portal);Materials.Add(MID);
        }
    }
    if(Style==2&&Source&&Source->Avatar)
    {
        auto GhostBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WarlockArena/Materials/M_BlinkEcho.M_BlinkEcho"));
        if(GhostBase)for(int I=0;I<2;++I)
        {
            auto Echo=NewObject<UPoseableMeshComponent>(this);Echo->SetupAttachment(RootComponent);Echo->SetCollisionEnabled(ECollisionEnabled::NoCollision);Echo->SetCastShadow(false);Echo->SetSkinnedAssetAndUpdate(Source->Avatar->GetSkeletalMeshAsset());Echo->RegisterComponent();Echo->CopyPoseFromSkeletalComponent(Source->Avatar);
            FTransform Transform=Source->Avatar->GetComponentTransform();Transform.SetScale3D(Source->AvatarRestScale);Transform.SetTranslation((I?End:GetActorLocation())+Source->Avatar->GetRelativeLocation());Echo->SetWorldTransform(Transform);
            auto MID=UMaterialInstanceDynamic::Create(GhostBase,this);MID->SetVectorParameterValue(TEXT("Tint"),Color);for(int J=0;J<Echo->GetNumMaterials();++J)Echo->SetMaterial(J,MID);Echoes.Add(Echo);Materials.Add(MID);
        }
    }
}
void AWarlockSpellFX::SetBeam(int Index,FVector A,FVector B,float Width)
{
    if(!Beams.IsValidIndex(Index))return;FVector Delta=B-A;
    Beams[Index]->SetWorldLocation((A+B)*.5f);Beams[Index]->SetWorldRotation(FQuat::FindBetweenNormals(FVector::UpVector,Delta.GetSafeNormal()));Beams[Index]->SetWorldScale3D(FVector(Width/50,Width/50,Delta.Size()/100));
}
void AWarlockSpellFX::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;
    if(Style>=11){if(Age>=Duration)Destroy();return;}
    if(Style==6&&Source&&HasAuthority())
    {if(Source->ChainTime<=0||!Source->bAlive){Destroy();return;}End=Source->ChainTarget.IsValid()?Source->ChainTarget->GetActorLocation():Source->ChainAnchor;}
    if(Style==7&&Source&&(Source->PendingCast!=Slot||Source->CastSerial!=SourceCastSerial||!Source->bAlive))Duration=FMath::Min(Duration,FMath::Max(0.f,Age-Dt)+.12f);
    if(Style==9&&Source&&(!Source->bAlive||Source->HasteRemaining<=0)){Destroy();return;}
    if(Age>=Duration){Destroy();return;}
    float Progress=FMath::Clamp(Age/Duration,0.f,1.f),Opacity=FMath::Square(1-Progress);
    if(Style==7)Opacity=FMath::Min(Age/.08f,1.f)*(1-Progress*.35f)*FMath::Clamp((Duration-Age)/.12f,0.f,1.f);
    if(Style==4||Style==6)Opacity=1;
    if(Style==9)Opacity=FMath::Min(Age/.15f,1.f)*FMath::Clamp((Duration-Age)/.3f,0.f,1.f)*.6f;
    for(auto MID:Materials){MID->SetScalarParameterValue(TEXT("Progress"),Progress);MID->SetScalarParameterValue(TEXT("Opacity"),Opacity);}
    for(int I=0;I<Portals.Num();++I)
    {
        float Scale=Style==7&&Source?FMath::Clamp(Source->CastRemaining/FMath::Max(.001f,Source->CastDuration),.02f,1.f):I==0?FMath::Square(1-Progress):FMath::Sin(FMath::Min(1.f,Progress*2)*PI*.5f)*(1-Progress*.55f);
        Portals[I]->SetWorldScale3D(FVector(FMath::Max(.01f,Scale)));
        if(Style==7&&Source)Portals[I]->SetWorldLocation(Source->GetActorLocation()+FVector(0,0,40));
    }
    for(int I=0;I<Echoes.Num();++I)if(Source){float Scale=I==0?FMath::Square(1-Progress):FMath::SmoothStep(.05f,1.f,Progress);Echoes[I]->SetWorldScale3D(Source->AvatarRestScale*FVector(FMath::Max(.01f,Scale),FMath::Max(.01f,Scale),.5f+.5f*Scale));}
    if(Ring)
    {
        float Radius=Style==0?Size*FMath::Clamp(Age/.19f,.12f,1.f):Style==4?Size:Style==7?45.f:Size*(.4f+.6f*Progress);
        Ring->SetWorldScale3D(FVector(Radius/50,Radius/50,1));Ring->SetWorldRotation(FRotator(0,Age*(Style==7?80:15),0));
        if(Style==7&&Source)Ring->SetWorldLocation(Source->GetActorLocation()+FVector(0,0,-36));
        if(Style==9&&Source)Ring->SetWorldLocation(Source->GetActorLocation()+FVector(0,0,-36));
    }
    for(int I=0;I<Particles.Num();++I)
    {
        auto& S=States[I];FVector Position=S.Position+S.Velocity*Age+FVector(0,0,-45*Age*Age);
        if(Style==7&&Source){float Angle=Age*(Slot==2?18.f:Slot==3?-7.f:9.f)+I*2*PI/Particles.Num();FVector Staff=Source->Avatar->GetSocketLocation(TEXT("16_Staff"));
            if(Slot==1||Slot==5){float Lift=FMath::Frac(Age*1.8f+I*.13f);Position=FMath::Lerp(Source->GetActorLocation()+FVector(FMath::Cos(Angle)*30,FMath::Sin(Angle)*30,-30),Staff,Lift);}
            else if(Slot==2)Position=Staff+FVector(FMath::Cos(Angle)*19,FMath::Sin(Angle*3)*8,FMath::Sin(Angle)*19);
            else if(Slot==3)Position=Staff+FVector(FMath::Cos(Angle)*23,FMath::Sin(Angle)*23,12*FMath::Sin(Angle*2));
            else Position=Staff+FVector(FMath::Cos(Angle)*14,FMath::Sin(Angle)*14,6*FMath::Sin(Angle*2));}
        if(Style==9&&Source){float Angle=Age*5+I*2*PI/Particles.Num();Position=Source->GetActorLocation()+FVector(FMath::Cos(Angle)*(38+I%3*8),FMath::Sin(Angle)*(38+I%3*8),-25+FMath::Frac(Age*.8f+I*.13f)*90);}
        if(Style==2){float Angle=Age*24+I*2*PI/Particles.Num();float Radius=I%2?50*FMath::Sin(Progress*PI):50*(1-Progress);Position=(I%2?End:GetActorLocation())+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,35+FMath::Sin(Angle*2)*45);}
        if(Style==4){float Angle=I*2*PI/Particles.Num()+Age*3;float Height=(1-Progress)*600;Position=GetActorLocation()+FVector(FMath::Cos(Angle)*18,FMath::Sin(Angle)*18,Height+FMath::Sin(Angle*3)*20);}
        Particles[I]->SetWorldLocation(Position);Particles[I]->SetWorldScale3D(FVector(Style==4?1.8f:1-Progress*.6f));
    }
    if(!Beams.IsEmpty())
    {
        FVector Start=Style==6&&Source?Source->Avatar->GetSocketLocation(TEXT("16_Staff")):GetActorLocation();FVector Finish=End;Finish.Z+=Style==6?40:0;
        FVector Direction=(Finish-Start).GetSafeNormal(),Side=FVector::CrossProduct(Direction,FVector::UpVector).GetSafeNormal();int Segments=Style==6?20:24;
        for(int I=0;I<Segments;++I){float T0=float(I)/Segments,T1=float(I+1)/Segments;auto Point=[&](float T){return FMath::Lerp(Start,Finish,T)+Side*FMath::Sin(T*PI)*FMath::Sin(T*79+FMath::FloorToFloat(Age*35)*3)*(Style==6?8:18)+FVector(0,0,FMath::Sin(T*PI)*FMath::Cos(T*57+Age*30)*10);};SetBeam(I,Point(T0),Point(T1),Style==6?1.7f:(I%3==0?2.f:1.1f));}
        for(int I=Segments;I<Beams.Num();++I){float T=float(I-Segments+1)/14;FVector A=FMath::Lerp(Start,Finish,T);SetBeam(I,A,A+Side*(I%2?1:-1)*30+FVector(0,0,20*FMath::Sin(I*4.f)),.65f);}
    }
}
void AWarlockSpellFX::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(AWarlockSpellFX,Style);DOREPLIFETIME(AWarlockSpellFX,Slot);DOREPLIFETIME(AWarlockSpellFX,SourceCastSerial);DOREPLIFETIME(AWarlockSpellFX,Size);DOREPLIFETIME(AWarlockSpellFX,End);DOREPLIFETIME(AWarlockSpellFX,Source);}
