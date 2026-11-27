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
    UPROPERTY(Config, EditAnywhere, Category="Casting") TArray<float> Windups={.36f,.7f,.42f,.03f,.45f,.12f,.33f,.39f};
    // Family IDs from WarlockCatalog; mobility/fission casts need a visible
    // gesture even though Blink and falling Meteor intentionally release fast.
    UPROPERTY(Config,EditAnywhere,Category="Casting") TMap<int32,float> FamilyWindups={{11,.26f},{12,.28f},{15,.32f},{16,.36f},{17,.4f}};
    UPROPERTY(Config, EditAnywhere, Category="Projectiles", meta=(ClampMin="0.3",ClampMax="1.5")) float ProjectileSpeedScale=.78f;
    UPROPERTY(Config, EditAnywhere, Category="Projectiles") float FireballSpeedScale=.60f;
    UPROPERTY(Config, EditAnywhere, Category="AI") float ReactionMin=.45f;
    UPROPERTY(Config, EditAnywhere, Category="AI") float ReactionMax=.75f;
    UPROPERTY(Config, EditAnywhere, Category="AI") float ActionInterval=.5f;
    float Windup(int Slot) const{return Windups.IsValidIndex(Slot)?FMath::Max(0.f,Windups[Slot]):.2f;}
};
