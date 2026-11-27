#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WarlockEconomy.generated.h"

// 1.00b globals + BN/VN/EN. These are room configuration values in the
// original map; defaults must not silently introduce placement/kill bounties.
UCLASS(Config=Game,DefaultConfig)
class WARLOCKARENA_API UWarlockEconomy : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(Config,EditAnywhere,meta=(ClampMin="0")) int StartingGold=20;
    UPROPERTY(Config,EditAnywhere,meta=(ClampMin="0")) int RoundSupply=10;
    UPROPERTY(Config,EditAnywhere,meta=(ClampMin="0")) int KillGold=0;
    UPROPERTY(Config,EditAnywhere,meta=(ClampMin="0")) int AssistGold=0;
    UPROPERTY(Config,EditAnywhere,meta=(ClampMin="0")) int VictoryGold=0;
    UPROPERTY(Config,EditAnywhere,meta=(ClampMin="0")) int TopDamageGold=0;
};
