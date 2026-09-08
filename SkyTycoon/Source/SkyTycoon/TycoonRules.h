#pragma once
#include "CoreMinimal.h"
#include "TycoonRules.generated.h"

UENUM(BlueprintType)
enum class ETycoonTile : uint8 { Start, Property, Event, Tax, Rest };
UENUM(BlueprintType)
enum class ETycoonPhase : uint8 { Roll, Purchase, End, Finished };
UENUM(BlueprintType)
enum class ETycoonDifficulty : uint8 { Easy, Normal, Hard };

USTRUCT(BlueprintType)
struct FTycoonTile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ETycoonTile Type = ETycoonTile::Property;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FIntPoint Cell = FIntPoint::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Price = 1000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Rent = 200;
};

USTRUCT(BlueprintType)
struct FTycoonMap
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name = TEXT("Sunny Ring");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 StartCash = 12000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Salary = 1500;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTycoonTile> Tiles;
    bool Validate(FString& Error) const;
    static FTycoonMap Default();
};

USTRUCT(BlueprintType)
struct FTycoonPlayer
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) int32 Cash = 12000;
    UPROPERTY(BlueprintReadOnly) int32 Position = 0;
    UPROPERTY(BlueprintReadOnly) bool bBankrupt = false;
    UPROPERTY(BlueprintReadOnly) bool bBot = false;
};

// Serializable state: contains no actor pointers, platform APIs, or client RNG.
USTRUCT(BlueprintType)
struct FTycoonMatch
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FTycoonMap Map;
    UPROPERTY(BlueprintReadOnly) TArray<FTycoonPlayer> Players;
    UPROPERTY(BlueprintReadOnly) TArray<int32> Owners;
    UPROPERTY(BlueprintReadOnly) int32 Turn = 0;
    UPROPERTY(BlueprintReadOnly) int32 Round = 1;
    UPROPERTY(BlueprintReadOnly) int32 LastDie = 0;
    UPROPERTY(BlueprintReadOnly) int32 Winner = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) ETycoonPhase Phase = ETycoonPhase::Roll;
    UPROPERTY(BlueprintReadOnly) FString Message;
    bool Start(const FTycoonMap& InMap, int32 PlayerCount);
    bool Roll(int32 Actor, int32 Die, int32 EventCash);
    bool Buy(int32 Actor);
    bool EndTurn(int32 Actor);
    bool BotShouldBuy(ETycoonDifficulty Difficulty) const;
private:
    void SettleBankruptcy();
};
