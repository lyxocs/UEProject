#pragma once
#include "CoreMinimal.h"
#include "TycoonRules.generated.h"

UENUM(BlueprintType)
enum class ETycoonTile : uint8 { Start, Property, Event, Tax, Rest, Shop, Jail, Hospital, GoToJail, Inn };
UENUM(BlueprintType)
enum class ETycoonPhase : uint8 { Roll, Purchase, End, Finished, Shopping, Detained, Lodging };
UENUM(BlueprintType)
enum class ETycoonDifficulty : uint8 { Easy, Normal, Hard };
UENUM(BlueprintType)
enum class ETycoonItem : uint8 { Bomb, Roadblock, Car, Shield, RemoteDice, Medkit, JailPass, Sweeper, RentPass, Count };
UENUM(BlueprintType)
enum class ETycoonTrap : uint8 { None, Bomb, Roadblock };
UENUM(BlueprintType)
enum class ETycoonStay : uint8 { None, Jail, Hospital, Inn };

struct FTycoonCatalog
{
    static constexpr int32 Capacity = 8;
    static constexpr int32 Bail = 800;
    static constexpr int32 Treatment = 600;
    static constexpr int32 InnFee = 300;
    static int32 Price(ETycoonItem Item);
    static FString Name(ETycoonItem Item);
    static FString Help(ETycoonItem Item);
};

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
    UPROPERTY(BlueprintReadOnly) TArray<int32> Inventory;
    UPROPERTY(BlueprintReadOnly) ETycoonStay Stay = ETycoonStay::None;
    UPROPERTY(BlueprintReadOnly) int32 StayTurns = 0;
    UPROPERTY(BlueprintReadOnly) int32 CarRolls = 0;
    UPROPERTY(BlueprintReadOnly) bool bShield = false;
    UPROPERTY(BlueprintReadOnly) bool bRentPass = false;
    UPROPERTY(BlueprintReadOnly) int32 ForcedDie = 0;
    int32 ItemCount(ETycoonItem Item) const;
    int32 InventorySize() const;
    int32 MaxDice() const { return CarRolls > 0 ? 3 : 2; }
};

// Serializable state: contains no actor pointers, platform APIs, or client RNG.
USTRUCT(BlueprintType)
struct FTycoonMatch
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FTycoonMap Map;
    UPROPERTY(BlueprintReadOnly) TArray<FTycoonPlayer> Players;
    UPROPERTY(BlueprintReadOnly) TArray<int32> Owners;
    UPROPERTY(BlueprintReadOnly) TArray<ETycoonTrap> Traps;
    UPROPERTY(BlueprintReadOnly) bool bUsedItem = false;
    UPROPERTY(BlueprintReadOnly) bool bShopped = false;
    UPROPERTY(BlueprintReadOnly) int32 Turn = 0;
    UPROPERTY(BlueprintReadOnly) int32 Round = 1;
    UPROPERTY(BlueprintReadOnly) int32 LastDie = 0;
    UPROPERTY(BlueprintReadOnly) int32 Winner = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) ETycoonPhase Phase = ETycoonPhase::Roll;
    UPROPERTY(BlueprintReadOnly) FString Message;
    bool Start(const FTycoonMap& InMap, int32 PlayerCount);
    bool Roll(int32 Actor, int32 Die, int32 EventCash, int32 SecondDie = 0, int32 ThirdDie = 0);
    bool BuyItem(int32 Actor, ETycoonItem Item);
    bool UseItem(int32 Actor, ETycoonItem Item, int32 Target = INDEX_NONE);
    bool Recover(int32 Actor, bool bPay);
    bool CheckIn(int32 Actor);
    bool BotUseItem();
    bool BotShop(ETycoonDifficulty Difficulty);
    bool Buy(int32 Actor);
    bool EndTurn(int32 Actor);
    bool BotShouldBuy(ETycoonDifficulty Difficulty) const;
private:
    void SettleBankruptcy();
    bool IsActor(int32 Actor) const;
    void ResolveTile(int32 EventCash);
    void SendTo(ETycoonStay Stay, int32 Turns);
    bool CanPlaceTrap(int32 Target) const;
};
