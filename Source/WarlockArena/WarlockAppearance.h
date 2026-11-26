#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WarlockAppearance.generated.h"

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
