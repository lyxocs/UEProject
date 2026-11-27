#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WarlockAppearance.generated.h"

namespace WarlockVisual
{
inline FLinearColor SeatColor(int Index)
{
    const FLinearColor Colors[]={FLinearColor(.30f,.055f,.035f),FLinearColor(.035f,.13f,.30f),FLinearColor(.045f,.23f,.085f),FLinearColor(.20f,.065f,.29f),FLinearColor(.33f,.21f,.035f),FLinearColor(.025f,.23f,.23f),FLinearColor(.30f,.065f,.16f),FLinearColor(.30f,.29f,.23f)};
    return Colors[FMath::Clamp(Index,0,7)];
}
}

USTRUCT(BlueprintType)
struct FWarlockLook
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Cloth = FLinearColor(.09f,.036f,.032f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Metal = FLinearColor(.26f,.19f,.11f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Glow = FLinearColor(.55f,.16f,.025f);
};

// Art-only parameters. No gameplay values belong in this asset.
UCLASS(BlueprintType)
class WARLOCKARENA_API UWarlockAppearanceSet : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FWarlockLook> Looks;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor SkinTone = FLinearColor(.35f,.24f,.19f);
};
