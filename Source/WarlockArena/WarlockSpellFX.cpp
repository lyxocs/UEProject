#include "WarlockSpellFX.h"
#include "WarlockGame.h"
#include "WarlockTuning.h"
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
    Duration=Style==4?1.15f:Style==6?30.f:Style==7?(Source?Source->CastDuration:.7f)+.2f:Style==1?.3f:Style==2?.55f:.65f;
    if(Style==7)Color=SpellColor(Slot);else if(Style==1||Style==2||Style==5||Style==6)Color=SpellColor(Style==1?2:Style==2?3:Style==5?6:7);else Color=SpellColor(Style==0?1:5);
    if(GetNetMode()==NM_DedicatedServer){SetLifeSpan(Duration+1);return;}
    auto SpriteBase=LoadObject<UMaterialInterface>(nullptr,Style==4?TEXT("/Game/WarlockArena/Materials/M_FlameTongue.M_FlameTongue"):TEXT("/Game/WarlockArena/Materials/M_ArcaneSprite.M_ArcaneSprite"));
    auto RingBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WarlockArena/Materials/M_ArcaneRing.M_ArcaneRing"));
    auto BeamBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WarlockArena/Materials/M_Glow.M_Glow"));
    auto Plane=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
    auto Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if(RingBase&&Style!=1&&Style!=6)
    {
        Ring=NewObject<UStaticMeshComponent>(this);Ring->SetupAttachment(RootComponent);Ring->SetStaticMesh(Plane);Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);Ring->SetCastShadow(false);Ring->RegisterComponent();
        auto MID=UMaterialInstanceDynamic::Create(RingBase,this);MID->SetVectorParameterValue(TEXT("Tint"),Color);Ring->SetMaterial(0,MID);Materials.Add(MID);
        Ring->SetWorldLocation(GetActorLocation()+FVector(0,0,-35));
    }
    const int Count=Style==1?18:Style==6?0:Style==7?12:Style==0?48:Style==4?24:32;
    if(SpriteBase)for(int I=0;I<Count;++I)
    {
        float A=2*PI*I/FMath::Max(1,Count);FVector Radial(FMath::Cos(A),FMath::Sin(A),0);
        FParticle State;State.Life=Duration*FMath::FRandRange(.55f,1.f);State.Size=I%4==0?2.5f:6.f;State.Position=GetActorLocation();
        if(Style==2)State.Position=FMath::Lerp(GetActorLocation(),End,float(I)/Count);
        if(Style==1)State.Position=FMath::Lerp(GetActorLocation(),End,float(I)/Count);
        State.Velocity=Radial*FMath::FRandRange(60.f,Size*2)+FVector(0,0,FMath::FRandRange(8.f,80.f));
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
    if(Style==2&&Source&&Source->Avatar)
    {
        auto GhostBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WarlockArena/Materials/M_BlinkEcho.M_BlinkEcho"));
        if(GhostBase)for(int I=0;I<3;++I)
        {
            auto Echo=NewObject<UPoseableMeshComponent>(this);Echo->SetupAttachment(RootComponent);Echo->SetCollisionEnabled(ECollisionEnabled::NoCollision);Echo->SetCastShadow(false);Echo->SetSkinnedAssetAndUpdate(Source->Avatar->GetSkeletalMeshAsset());Echo->RegisterComponent();Echo->CopyPoseFromSkeletalComponent(Source->Avatar);
            FTransform Transform=Source->Avatar->GetComponentTransform();Transform.SetTranslation(GetActorLocation()+Source->Avatar->GetRelativeLocation()+(End-GetActorLocation())*(I/3.f));Echo->SetWorldTransform(Transform);
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
    if(Style==6&&Source&&HasAuthority())
    {if(Source->ChainTime<=0||!Source->bAlive){Destroy();return;}End=Source->ChainTarget.IsValid()?Source->ChainTarget->GetActorLocation():Source->ChainAnchor;}
    if(Style==7&&Source&&(Source->PendingCast!=Slot||Source->CastSerial!=SourceCastSerial||!Source->bAlive))Duration=FMath::Min(Duration,Age+.12f);
    if(Age>=Duration){Destroy();return;}
    float Progress=FMath::Clamp(Age/Duration,0.f,1.f),Opacity=FMath::Square(1-Progress);
    if(Style==7)Opacity=FMath::Min(Age/.08f,1.f)*(1-Progress*.35f)*FMath::Clamp((Duration-Age)/.12f,0.f,1.f);
    if(Style==4||Style==6)Opacity=1;
    for(auto MID:Materials){MID->SetScalarParameterValue(TEXT("Progress"),Progress);MID->SetScalarParameterValue(TEXT("Opacity"),Opacity);}
    if(Ring)
    {
        float Radius=Style==0?Size*FMath::Clamp(Age/.19f,.12f,1.f):Style==4?Size:Style==7?45.f:Size*(.4f+.6f*Progress);
        Ring->SetWorldScale3D(FVector(Radius/50,Radius/50,1));Ring->SetWorldRotation(FRotator(0,Age*(Style==7?80:15),0));
        if(Style==7&&Source)Ring->SetWorldLocation(Source->GetActorLocation()+FVector(0,0,-36));
    }
    for(int I=0;I<Particles.Num();++I)
    {
        auto& S=States[I];FVector Position=S.Position+S.Velocity*Age+FVector(0,0,-45*Age*Age);
        if(Style==7&&Source){float Angle=Age*9+I*2*PI/Particles.Num();Position=Source->Avatar->GetSocketLocation(TEXT("16_Staff"))+FVector(FMath::Cos(Angle)*14,FMath::Sin(Angle)*14,6*FMath::Sin(Angle*2));}
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
