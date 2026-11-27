#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WarlockTuning.generated.h"

// Current feel preset. Smite's 0.7s is sourced from S001 acas/Ncl1;
// other anticipation times and the slower projectile preset are UE decisions.
UCLASS(Config=Game, DefaultConfig)
class WARLOCKARENA_API UWarlockTuning : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Casting") TArray<float> Windups={.36f,.7f,.42f,.09f,.45f,.12f,.33f,.39f};
    UPROPERTY(Config, EditAnywhere, Category="Projectiles", meta=(ClampMin="0.3",ClampMax="1.5")) float ProjectileSpeedScale=.78f;
    UPROPERTY(Config, EditAnywhere, Category="Projectiles") float FireballSpeedScale=.60f;
    UPROPERTY(Config, EditAnywhere, Category="AI") float ReactionMin=.45f;
    UPROPERTY(Config, EditAnywhere, Category="AI") float ReactionMax=.75f;
    UPROPERTY(Config, EditAnywhere, Category="AI") float ActionInterval=.5f;
    float Windup(int Slot) const{return Windups.IsValidIndex(Slot)?FMath::Max(0.f,Windups[Slot]):.2f;}
};
