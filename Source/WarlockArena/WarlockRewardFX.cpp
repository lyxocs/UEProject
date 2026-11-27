#include "WarlockGame.h"
#include "WarlockAudio.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

AWarlockRewardFX::AWarlockRewardFX()
{
    bReplicates=true;bAlwaysRelevant=true;PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Celebration")));
    Ring=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RewardRune"));Ring->SetupAttachment(RootComponent);Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);Ring->SetCastShadow(false);
}
void AWarlockRewardFX::Spawn(AWarlockPawn* From,int InKind,int InGold,int InPoints,int InStreak)
{
    if(!From||!From->HasAuthority())return;
    if(auto M=From->GetWorld()->GetAuthGameMode<AWarlockMode>();M&&M->bTraining)return;
    FActorSpawnParameters Params;Params.bDeferConstruction=true;Params.Owner=From;
    auto FX=From->GetWorld()->SpawnActor<AWarlockRewardFX>(StaticClass(),From->GetActorLocation(),FRotator::ZeroRotator,Params);
    if(FX){FX->Source=From;FX->Kind=InKind;FX->Gold=InGold;FX->Points=InPoints;FX->Streak=InStreak;FX->Born=From->GetWorld()->GetGameState()->GetServerWorldTimeSeconds();FX->FinishSpawning(FTransform(From->GetActorLocation()));}
}
void AWarlockRewardFX::BeginPlay()
{
    Super::BeginPlay();Duration=Kind==2?2.4f:Kind==1?1.2f:1.6f;
    if(HasAuthority())SetLifeSpan(Duration+.4f);
    if(GetNetMode()==NM_DedicatedServer){SetActorTickEnabled(false);return;}
    Ring->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
    auto Rune=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WarlockArena/Materials/M_ArenaBoundary.M_ArenaBoundary"));
    if(Rune){auto MID=UMaterialInstanceDynamic::Create(Rune,this);MID->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1,.62f,.16f));Ring->SetMaterial(0,MID);}
    auto Sprite=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WarlockArena/Materials/M_ArcaneSprite.M_ArcaneSprite"));
    if(Sprite)
    {
        Glow=UMaterialInstanceDynamic::Create(Sprite,this);Glow->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1,.65f,.18f));
        for(int I=0;I<(Kind==2?12:6);++I){auto Spark=NewObject<UMaterialBillboardComponent>(this);Spark->SetupAttachment(RootComponent);Spark->SetAbsolute(true,true,true);Spark->SetCollisionEnabled(ECollisionEnabled::NoCollision);Spark->SetCastShadow(false);Spark->AddElement(Glow,nullptr,false,Kind==2?4.f:3.5f,Kind==2?11.f:9.f,nullptr);Spark->RegisterComponent();Sparks.Add(Spark);}
    }
    if(Source&&Source->IsPlayerControlled()&&Source->IsLocallyControlled())if(auto Audio=GetWorld()->GetSubsystem<UWarlockAudio>())Audio->PlayCelebration(Kind,Streak);
}
void AWarlockRewardFX::Tick(float Dt)
{
    Super::Tick(Dt);if(GetNetMode()==NM_DedicatedServer)return;
    if(auto GS=GetWorld()->GetGameState())Age=FMath::Max(0.f,GS->GetServerWorldTimeSeconds()-Born);else Age+=Dt;
    if(Age>Duration){Ring->SetVisibility(false);for(auto Spark:Sparks)Spark->SetVisibility(false);return;}
    float T=FMath::Clamp(Age/Duration,0.f,1.f),Fade=FMath::Square(1-T);
    if(Source)SetActorLocation(Source->DisplayLocation());
    const FVector O=GetActorLocation();Ring->SetWorldLocation(FVector(O.X,O.Y,6));Ring->SetWorldRotation(FRotator(0,Age*35,0));
    float Radius=(Kind==2?75.f:35.f)+(Kind==2?100.f:45.f)*(1-FMath::Exp(-Age*5));Ring->SetWorldScale3D(FVector(Radius/50,Radius/50,1));
    if(auto MID=Cast<UMaterialInstanceDynamic>(Ring->GetMaterial(0)))MID->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1,.62f,.16f)*Fade);
    if(Glow)Glow->SetScalarParameterValue(TEXT("Opacity"),Fade);
    for(int I=0;I<Sparks.Num();++I){float A=I*2*PI/Sparks.Num()+Age*(I%2?1.6f:-1.2f);Sparks[I]->SetWorldLocation(O+FVector(FMath::Cos(A)*(30+Age*18),FMath::Sin(A)*(30+Age*18),10+Age*(Kind==2?100.f:65.f)+I%3*12));}
}
void AWarlockRewardFX::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(AWarlockRewardFX,Source);DOREPLIFETIME(AWarlockRewardFX,Kind);DOREPLIFETIME(AWarlockRewardFX,Gold);DOREPLIFETIME(AWarlockRewardFX,Points);DOREPLIFETIME(AWarlockRewardFX,Streak);DOREPLIFETIME(AWarlockRewardFX,Born);}
