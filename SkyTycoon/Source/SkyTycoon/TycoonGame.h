#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "TycoonRules.h"
#include "TycoonGame.generated.h"

UCLASS()
class ATycoonController : public APlayerController
{
    GENERATED_BODY()
public:
    ATycoonController();
    virtual void SetupInputComponent() override;
    void Primary();
    void Skip();
    void Restart();
    void Click();
    void Touch(ETouchIndex::Type Finger, FVector Location);
};

UCLASS()
class ATycoonGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ATycoonGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(BlueprintReadOnly) FTycoonMatch Match;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Board") FTycoonMap CustomMap;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Board", meta=(ClampMin="2", ClampMax="4")) int32 PlayerCount = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Board", meta=(ClampMin="0", ClampMax="3")) int32 BotCount = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Board") ETycoonDifficulty BotDifficulty = ETycoonDifficulty::Normal;
    UPROPERTY(BlueprintReadOnly) bool bFastBots = false;
    UPROPERTY(BlueprintReadOnly) ETycoonItem SelectedItem = ETycoonItem::Bomb;
    UPROPERTY(BlueprintReadOnly) int32 SelectedTarget = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) int32 SelectedDie = 1;
    UPROPERTY(BlueprintReadOnly) int32 DiceMask = 3;
    void ToggleDie(int32 Index);
    int32 SelectedDiceCount() const;
    void UseSelected();
    void ToggleBotSpeed();
    float BotInterval() const { return bFastBots ? .4f : .8f; }
    void Primary();
    void Skip();
    void Restart();
private:
    float BotElapsed = 0.f;
    void AdvanceAction();
};

UCLASS()
class ATycoonHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    void ActivateAt(float X, float Y);
private:
    float UIScale = 1.f;
    FVector2D UIOrigin = FVector2D::ZeroVector;
    TArray<FBox2D> TileBoxes;
    TArray<FBox2D> ItemBoxes;
    TArray<FBox2D> DiceBoxes;
    TArray<FBox2D> RollDiceBoxes;
    FBox2D PrimaryBox, SkipBox, UseBox, SpeedBox;
};
