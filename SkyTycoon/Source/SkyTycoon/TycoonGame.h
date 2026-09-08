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
    FBox2D PrimaryBox;
    FBox2D SkipBox;
};
