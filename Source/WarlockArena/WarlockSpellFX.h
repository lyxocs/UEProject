#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WarlockSpellFX.generated.h"

class AWarlockPawn;
class UStaticMeshComponent;
class UMaterialBillboardComponent;
class UMaterialInstanceDynamic;
class UPoseableMeshComponent;

// Visual events only. Damage, cooldowns and collision never depend on this actor.
UCLASS()
class WARLOCKARENA_API AWarlockSpellFX : public AActor
{
    GENERATED_BODY()
public:
    AWarlockSpellFX();
    // 0 Smite, 1 lightning, 2 blink, 4 meteor warning, 5 shield,
    // 6 live chain, 7 anticipation, 8 impact.
    UPROPERTY(Replicated, BlueprintReadOnly) int Style=8;
    UPROPERTY(Replicated, BlueprintReadOnly) int Slot=0;
    UPROPERTY(Replicated, BlueprintReadOnly) int SourceCastSerial=0;
    UPROPERTY(Replicated, BlueprintReadOnly) float Size=100;
    UPROPERTY(Replicated, BlueprintReadOnly) FVector End=FVector::ZeroVector;
    UPROPERTY(Replicated, BlueprintReadOnly) TObjectPtr<AWarlockPawn> Source;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UMaterialBillboardComponent>> Particles;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UStaticMeshComponent>> Beams;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UPoseableMeshComponent>> Echoes;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UMaterialBillboardComponent>> Portals;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Ring;
    struct FParticle {FVector Position,Velocity;float Life,Size;};
    TArray<FParticle> States;
    float Age=0,Duration=.55f;
    FLinearColor Color=FLinearColor(1,.3f,.03f);
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    static AWarlockSpellFX* Spawn(UWorld* World,int Style,FVector Position,FVector End,float Size,AWarlockPawn* Source=nullptr,int Slot=0);
    void SetBeam(int Index,FVector A,FVector B,float Width);
};
