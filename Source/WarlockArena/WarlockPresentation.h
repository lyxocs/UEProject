#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WarlockPresentation.generated.h"

USTRUCT(BlueprintType)
struct FWarlockArenaPalette
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FString Name;
    UPROPERTY(EditAnywhere) FLinearColor Stone=FLinearColor::White;
    UPROPERTY(EditAnywhere) FLinearColor LavaCore=FLinearColor(.52f,.13f,.018f);
    UPROPERTY(EditAnywhere) FLinearColor LavaBank=FLinearColor(.24f,.035f,.004f);
    UPROPERTY(EditAnywhere) FLinearColor Accent=FLinearColor(.62f,.44f,.21f);
};

// Presentation only: changing these never changes collision, damage or economy.
UCLASS(Config=Game, DefaultConfig)
class WARLOCKARENA_API UWarlockPresentation : public UObject
{
    GENERATED_BODY()
public:
    UWarlockPresentation();
    UPROPERTY(Config, EditAnywhere) TArray<FWarlockArenaPalette> Palettes;
    UPROPERTY(Config, EditAnywhere, meta=(ClampMin="0.1")) float PaletteBlendSpeed=1.4f;
    UPROPERTY(Config, EditAnywhere, meta=(ClampMin="0.05")) float HitFlashSeconds=.18f;
    static FWarlockArenaPalette Palette(int Index);
    static int PaletteForRound(int Round,const FString& Phase);
};
