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
}
void ATycoonGameMode::Primary()
{
    if (Match.Players.IsValidIndex(Match.Turn) && !Match.Players[Match.Turn].bBot) AdvanceAction();
}
void ATycoonGameMode::AdvanceAction()
{
    if (Match.Phase == ETycoonPhase::Roll) Match.Roll(Match.Turn, FMath::RandRange(1,6), FMath::RandBool() ? 500 : -400);
    else if (Match.Phase == ETycoonPhase::Purchase) Match.Buy(Match.Turn);
    else if (Match.Phase == ETycoonPhase::End) Match.EndTurn(Match.Turn);
}
void ATycoonGameMode::Skip() { if (Match.Players.IsValidIndex(Match.Turn) && !Match.Players[Match.Turn].bBot) Match.EndTurn(Match.Turn); }
void ATycoonGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Match.Phase == ETycoonPhase::Finished || !Match.Players.IsValidIndex(Match.Turn) || !Match.Players[Match.Turn].bBot)
    { BotElapsed = 0.f; return; }
    BotElapsed += DeltaSeconds;
    if (BotElapsed < .8f) return;
    BotElapsed = 0.f;
    if (Match.Phase == ETycoonPhase::Purchase && !Match.BotShouldBuy(BotDifficulty)) Match.EndTurn(Match.Turn);
    else AdvanceAction();
}

void ATycoonHUD::DrawHUD()
{
    Super::DrawHUD();
    const auto* G = GetWorld()->GetAuthGameMode<ATycoonGameMode>();
    if (!Canvas || !G || G->Match.Players.IsEmpty()) return;
    const auto& S = G->Match;
    const float W = Canvas->SizeX, H = Canvas->SizeY;
    DrawRect(FLinearColor(.035f,.065f,.11f),0,0,W,H);
    DrawText(TEXT("SKY TYCOON  /  LOCAL PROTOTYPE"),FLinearColor::White,20,16,nullptr,1.5f);
    const float Size = FMath::Max(70.f, FMath::Min(W-40,H-230));
    const float X0 = (W-Size)/2, Y0 = 52;
    int32 Extent = 1; for (const auto& T : S.Map.Tiles) Extent = FMath::Max(Extent,FMath::Max(T.Cell.X,T.Cell.Y)+1);
    const float Cell = Size / Extent;
    for (int32 I=0; I<S.Map.Tiles.Num(); ++I)
    {
        const auto& T = S.Map.Tiles[I]; const float X = X0+T.Cell.X*Cell, Y = Y0+T.Cell.Y*Cell;
        const int32 OwnerSeat = S.Owners[I];
        DrawRect(OwnerSeat == INDEX_NONE ? FLinearColor(.12f,.22f,.29f) : Colors[OwnerSeat]*.55f,X+2,Y+2,Cell-4,Cell-4);
        DrawText(T.Name,FLinearColor::White,X+5,Y+8,nullptr,FMath::Clamp(Cell/80,.5f,1.f));
        if (T.Type == ETycoonTile::Property) DrawText(FString::Printf(TEXT("$%d"),T.Price),FLinearColor(.65f,.85f,.8f),X+5,Y+Cell*.5f,nullptr,.7f);
        for (int32 P=0; P<S.Players.Num(); ++P) if (!S.Players[P].bBankrupt && S.Players[P].Position==I)
            DrawRect(Colors[P],X+5+P*10,Y+Cell-14,8,8);
    }
    const float InfoY = Y0+Size+8;
    DrawText(FString::Printf(TEXT("Round %d  |  %s  |  Dice %d"),S.Round,*S.Players[S.Turn].Name,S.LastDie),Colors[S.Turn],20,InfoY,nullptr,1.1f);
    FString Balances;
    for (const auto& P : S.Players) Balances += FString::Printf(TEXT("%s: $%d%s   "),*P.Name,P.Cash,P.bBankrupt ? TEXT(" OUT") : TEXT(""));
    DrawText(Balances,FLinearColor::White,20,InfoY+25,nullptr,.9f);
    DrawText(S.Message,FLinearColor(.7f,.8f,.9f),20,InfoY+48,nullptr,.9f);
    PrimaryBox = FBox2D(FVector2D(20,H-65),FVector2D(W*.65f,H-20));
    SkipBox = FBox2D(FVector2D(W*.68f,H-65),FVector2D(W-20,H-20));
    DrawRect(FLinearColor(.12f,.4f,.8f),20,H-65,W*.65f-20,45);
    DrawRect(FLinearColor(.2f,.27f,.35f),W*.68f,H-65,W*.32f-20,45);
    const FString Label = S.Phase == ETycoonPhase::Roll ? TEXT("ROLL [Space]") : S.Phase == ETycoonPhase::Purchase ? TEXT("BUY [Space]") : S.Phase == ETycoonPhase::Finished ? TEXT("FINISHED - R to restart") : TEXT("END TURN [Space]");
    DrawText(Label,FLinearColor::White,30,H-52,nullptr,1.f);
    DrawText(TEXT("PASS [E]"),FLinearColor::White,W*.68f+8,H-52,nullptr,1.f);
}
void ATycoonHUD::ActivateAt(float X,float Y)
{
    if (auto* G = GetWorld()->GetAuthGameMode<ATycoonGameMode>())
    { if (PrimaryBox.IsInside(FVector2D(X,Y))) G->Primary(); else if (SkipBox.IsInside(FVector2D(X,Y))) G->Skip(); }
}
