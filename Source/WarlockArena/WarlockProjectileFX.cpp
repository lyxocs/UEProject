#include "WarlockGame.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Scalability.h"

void AWarlockProjectile::InitializeFireParticles()
{
    LastVisualPosition=GetActorLocation();
    if(auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();M&&M->bTraining)return;
    if(GetNetMode()==NM_DedicatedServer)return;
    auto Material=LoadObject<UMaterialInterface>(nullptr,Kind==2?TEXT("/Game/WarlockArena/Materials/M_ArcaneSprite.M_ArcaneSprite"):TEXT("/Game/WarlockArena/Materials/M_FlameTongue.M_FlameTongue"));
    if(!Material)return;
    Mesh->SetVisibility(false);
    const int BaseCount=Kind==2?12:Kind==1?18:36;
    int Quality=Scalability::GetQualityLevels().EffectsQuality;
    const int Count=Quality>=2?BaseCount:6+FMath::Max(2,FMath::RoundToInt((BaseCount-6)*(Quality==0?.33f:.6f)));
    for(int I=0;I<Count;++I)
    {
        auto Sprite=NewObject<UMaterialBillboardComponent>(this);
        Sprite->SetupAttachment(VisualRoot);Sprite->SetAbsolute(true,true,true);Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);Sprite->SetCastShadow(false);
        auto MID=UMaterialInstanceDynamic::Create(Material,this);
        MID->SetVectorParameterValue(TEXT("Tint"),Kind==2?FLinearColor(.15f,.5f,1):I%5==0?FLinearColor(1,.52f,.11f):FLinearColor(1,.16f,.015f));
        MID->SetScalarParameterValue(TEXT("Seed"),I*.71f);
        MID->SetScalarParameterValue(TEXT("Opacity"),0);
        Sprite->AddElement(MID,nullptr,false,I<6?27.f:I%5==0?2.5f:16.f,I<6?39.f:I%5==0?3.f:32.f,nullptr);
        Sprite->RegisterComponent();FireParticles.Add(Sprite);FireParticleMaterials.Add(MID);FireParticleStates.AddDefaulted();
    }
}
void AWarlockProjectile::Tick(float Dt)
{
    Super::Tick(Dt);VisualAge+=Dt;
    Mesh->SetRelativeRotation(FRotator(VisualAge*630,VisualAge*210,VisualAge*470));
    Mesh->SetRelativeScale3D(FVector(Radius/50.f*.75f,Radius/50.f*.53f,Radius/50.f*.65f));
    if(FireParticles.IsEmpty())return;
    const FVector Current=GetActorLocation();
    const float Frame=FMath::Min(Dt,.1f);EmissionTime+=Frame;
    float EmissionInterval=.012f*float((Kind==2?12:Kind==1?18:36)-6)/FMath::Max(2,FireParticles.Num()-6);
    while(EmissionTime>=EmissionInterval)
    {
        EmissionTime-=EmissionInterval;int I=6+NextParticle++%FMath::Max(1,FireParticles.Num()-6);auto& State=FireParticleStates[I];bool Ember=I%5==0;
        State.Age=0;State.Life=Ember?.5f:.34f;State.Size=Ember?1.f:FMath::FRandRange(.8f,1.3f)*(Kind==1?.65f:1.f);
        FVector Direction=(Current-LastVisualPosition).GetSafeNormal2D();
        State.Position=FMath::Lerp(LastVisualPosition,Current,FMath::FRand())+FVector(FMath::FRandRange(-9.f,9.f),FMath::FRandRange(-9.f,9.f),FMath::FRandRange(-5.f,9.f));
        State.Velocity=-Direction*FMath::FRandRange(20.f,65.f)+FVector(FMath::FRandRange(-30.f,30.f),FMath::FRandRange(-30.f,30.f),Ember?FMath::FRandRange(45.f,110.f):25);
    }
    for(int I=0;I<FireParticles.Num();++I)
    {
        if(I<6)
        {
            float Angle=VisualAge*(I%2?6:-8)+I*2*PI/6;float Scale=Kind==1?.65f:Kind==2?.4f:1.f;
            FireParticles[I]->SetWorldLocation(Current+FVector(FMath::Cos(Angle)*13,FMath::Sin(Angle)*13,FMath::Sin(Angle*2)*9)*Scale);
            FireParticles[I]->SetWorldScale3D(FVector(Scale*(.85f+.15f*FMath::Sin(VisualAge*14+I))));FireParticleMaterials[I]->SetScalarParameterValue(TEXT("Opacity"),.52f+.10f*FMath::Sin(VisualAge*11+I));continue;
        }
        auto& State=FireParticleStates[I];State.Age+=Frame;float Fade=FMath::Clamp(1-State.Age/State.Life,0.f,1.f);
        if(Fade>0){State.Position+=State.Velocity*Frame;FireParticles[I]->SetWorldLocation(State.Position);FireParticles[I]->SetWorldScale3D(FVector(State.Size*(.4f+.6f*Fade)));}
        FireParticleMaterials[I]->SetScalarParameterValue(TEXT("Opacity"),Fade*Fade);
    }
    LastVisualPosition=Current;
}
