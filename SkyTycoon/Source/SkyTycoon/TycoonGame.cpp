#include "TycoonGame.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"

namespace { const FLinearColor Colors[] = { FLinearColor(1,.3f,.4f), FLinearColor(.2f,.65f,1), FLinearColor(1,.75f,.2f), FLinearColor(.65f,.4f,1) }; }

ATycoonController::ATycoonController() { bShowMouseCursor = true; }
void ATycoonController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ATycoonController::Primary);
    InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ATycoonController::Skip);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ATycoonController::Restart);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ATycoonController::Click);
    InputComponent->BindTouch(IE_Pressed, this, &ATycoonController::Touch);
}
void ATycoonController::Primary() { if (auto* G = GetWorld()->GetAuthGameMode<ATycoonGameMode>()) G->Primary(); }
void ATycoonController::Skip() { if (auto* G = GetWorld()->GetAuthGameMode<ATycoonGameMode>()) G->Skip(); }
void ATycoonController::Restart() { if (auto* G = GetWorld()->GetAuthGameMode<ATycoonGameMode>()) G->Restart(); }
void ATycoonController::Click() { float X, Y; if (GetMousePosition(X,Y)) if (auto* H = Cast<ATycoonHUD>(GetHUD())) H->ActivateAt(X,Y); }
void ATycoonController::Touch(ETouchIndex::Type Finger, FVector Location) { if (Finger == ETouchIndex::Touch1) if (auto* H = Cast<ATycoonHUD>(GetHUD())) H->ActivateAt(Location.X,Location.Y); }

ATycoonGameMode::ATycoonGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PlayerControllerClass = ATycoonController::StaticClass(); HUDClass = ATycoonHUD::StaticClass();
    DefaultPawnClass = nullptr;
}
void ATycoonGameMode::BeginPlay() { Super::BeginPlay(); Restart(); }
void ATycoonGameMode::Restart()
{
    const auto Map = CustomMap.Tiles.IsEmpty() ? FTycoonMap::Default() : CustomMap;
    if (!Match.Start(Map, PlayerCount)) { Match.Start(FTycoonMap::Default(), 2); Match.Message = TEXT("Invalid custom map; using default."); }
    const int32 Bots = FMath::Clamp(BotCount, 0, Match.Players.Num()-1);
    for (int32 I = Match.Players.Num()-Bots; I < Match.Players.Num(); ++I)
    { Match.Players[I].bBot = true; Match.Players[I].Name = FString::Printf(TEXT("Bot %d"),I); }
    BotElapsed = 0.f;
    SelectedTarget = INDEX_NONE;
    DiceMask = 3;
}
void ATycoonGameMode::Primary()
{
    if (Match.Players.IsValidIndex(Match.Turn) && !Match.Players[Match.Turn].bBot) AdvanceAction();
}
void ATycoonGameMode::AdvanceAction()
{
    if (Match.Phase == ETycoonPhase::Roll)
    {
        const int32 Count = Match.Players[Match.Turn].bBot ? Match.Players[Match.Turn].MaxDice() : SelectedDiceCount();
        Match.Roll(Match.Turn, FMath::RandRange(1,6), FMath::RandBool() ? 500 : -400, Count>=2 ? FMath::RandRange(1,6) : 0, Count>=3 ? FMath::RandRange(1,6) : 0);
    }
    else if (Match.Phase == ETycoonPhase::Purchase) Match.Buy(Match.Turn);
    else if (Match.Phase == ETycoonPhase::End) Match.EndTurn(Match.Turn);
    else if (Match.Phase == ETycoonPhase::Shopping)
    {
        if (Match.bShopped) Match.EndTurn(Match.Turn);
        else if (!Match.BuyItem(Match.Turn,SelectedItem)) Match.Message = TEXT("Cannot buy: one per visit, bag max 8, or insufficient cash.");
    }
    else if (Match.Phase == ETycoonPhase::Lodging)
    {
        if (!Match.CheckIn(Match.Turn)) Match.Message = TEXT("Inn requires $300. You may leave instead.");
    }
    else if (Match.Phase == ETycoonPhase::Detained)
    {
        if (Match.Players[Match.Turn].Stay == ETycoonStay::Inn) Match.Recover(Match.Turn,false);
        else if (!Match.Recover(Match.Turn,true)) Match.Message = TEXT("Insufficient cash for release. Use a card or wait.");
    }
}
void ATycoonGameMode::Skip()
{
    if (!Match.Players.IsValidIndex(Match.Turn) || Match.Players[Match.Turn].bBot) return;
    if (Match.Phase == ETycoonPhase::Detained) Match.Recover(Match.Turn,false);
    else Match.EndTurn(Match.Turn);
}
void ATycoonGameMode::ToggleBotSpeed() { bFastBots = !bFastBots; }
int32 ATycoonGameMode::SelectedDiceCount() const
{
    const int32 Max = Match.Players.IsValidIndex(Match.Turn) ? Match.Players[Match.Turn].MaxDice() : 2;
    const int32 Mask = DiceMask & ((1<<Max)-1);
    return FMath::Max(1,(Mask&1)+((Mask>>1)&1)+((Mask>>2)&1));
}
void ATycoonGameMode::ToggleDie(int32 Index)
{
    if (!Match.Players.IsValidIndex(Match.Turn) || Match.Players[Match.Turn].bBot || Match.Phase!=ETycoonPhase::Roll || Index<0 || Index>=Match.Players[Match.Turn].MaxDice()) return;
    DiceMask &= (1<<Match.Players[Match.Turn].MaxDice())-1;
    if (DiceMask==0) DiceMask=1;
    const int32 Next=DiceMask^(1<<Index);
    if (Next!=0) DiceMask=Next;
}
void ATycoonGameMode::UseSelected()
{
    if (!Match.Players.IsValidIndex(Match.Turn) || Match.Players[Match.Turn].bBot) return;
    if (!Match.UseItem(Match.Turn,SelectedItem,SelectedItem==ETycoonItem::RemoteDice ? SelectedDie : SelectedTarget))
        Match.Message = TEXT("Cannot use: check phase, stock, target, effect already active, or one-item limit.");
    else if (SelectedItem==ETycoonItem::Car) DiceMask=7;
}
void ATycoonGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Match.Players.IsValidIndex(Match.Turn))
    {
        DiceMask &= (1<<Match.Players[Match.Turn].MaxDice())-1;
        if (DiceMask==0) DiceMask=1;
    }
    if (Match.Phase == ETycoonPhase::Finished || !Match.Players.IsValidIndex(Match.Turn) || !Match.Players[Match.Turn].bBot)
    { BotElapsed = 0.f; return; }
    BotElapsed += DeltaSeconds;
    if (BotElapsed < BotInterval()) return;
    BotElapsed = 0.f;
    if (Match.BotUseItem()) return;
    if (Match.Phase == ETycoonPhase::Detained)
    {
        const auto& P = Match.Players[Match.Turn];
        const int32 Fee = P.Stay == ETycoonStay::Jail ? FTycoonCatalog::Bail : FTycoonCatalog::Treatment;
        Match.Recover(Match.Turn,P.Stay != ETycoonStay::Inn && P.Cash >= Fee+1500);
    }
    else if (Match.Phase == ETycoonPhase::Shopping) { if (!Match.BotShop(BotDifficulty)) Match.EndTurn(Match.Turn); }
    else if (Match.Phase == ETycoonPhase::Lodging)
    {
        if (Match.Players[Match.Turn].bShield || Match.Players[Match.Turn].Cash < 1800) Match.EndTurn(Match.Turn);
        else Match.CheckIn(Match.Turn);
    }
    else if (Match.Phase == ETycoonPhase::Purchase && !Match.BotShouldBuy(BotDifficulty)) Match.EndTurn(Match.Turn);
    else AdvanceAction();
}
