#include "TycoonRules.h"

bool FTycoonMap::Validate(FString& Error) const
{
    if (Tiles.Num() < 8 || Tiles.Num() > 128 || StartCash < 1 || StartCash > 1000000 || Salary < 0 || Salary > 100000)
    { Error = TEXT("Map needs 8-128 tiles and bounded cash settings."); return false; }
    TSet<FIntPoint> Cells;
    for (int32 I = 0; I < Tiles.Num(); ++I)
    {
        const auto& T = Tiles[I];
        if ((I == 0) != (T.Type == ETycoonTile::Start) || T.Name.IsEmpty() || T.Name.Len() > 32 ||
            T.Cell.X < 0 || T.Cell.Y < 0 || T.Cell.X > 15 || T.Cell.Y > 15 || Cells.Contains(T.Cell) ||
            T.Price < 0 || T.Price > 1000000 || T.Rent < 0 || T.Rent > 100000 ||
            static_cast<uint8>(T.Type) > static_cast<uint8>(ETycoonTile::Rest))
        { Error = TEXT("Invalid tile, duplicate cell, or start is not tile zero."); return false; }
        Cells.Add(T.Cell);
        const auto& Next = Tiles[(I + 1) % Tiles.Num()];
        if (FMath::Abs(T.Cell.X - Next.Cell.X) + FMath::Abs(T.Cell.Y - Next.Cell.Y) != 1)
        { Error = TEXT("Tiles must form a closed adjacent path."); return false; }
    }
    Error.Reset(); return true;
}

FTycoonMap FTycoonMap::Default()
{
    FTycoonMap M;
    for (int32 I = 0; I < 24; ++I)
    {
        FTycoonTile T;
        if (I < 6) T.Cell = FIntPoint(I, 0);
        else if (I < 12) T.Cell = FIntPoint(6, I - 6);
        else if (I < 18) T.Cell = FIntPoint(18 - I, 6);
        else T.Cell = FIntPoint(0, 24 - I);
        T.Type = I == 0 ? ETycoonTile::Start : I % 6 == 0 ? ETycoonTile::Rest : I % 5 == 0 ? ETycoonTile::Tax : I % 4 == 0 ? ETycoonTile::Event : ETycoonTile::Property;
        T.Name = I == 0 ? TEXT("START") : T.Type == ETycoonTile::Property ? FString::Printf(TEXT("Lot %02d"), I) : T.Type == ETycoonTile::Tax ? TEXT("TAX") : T.Type == ETycoonTile::Event ? TEXT("CHANCE") : TEXT("PARK");
        T.Price = 800 + I * 100; T.Rent = T.Price / 5;
        M.Tiles.Add(T);
    }
    return M;
}

bool FTycoonMatch::Start(const FTycoonMap& InMap, int32 PlayerCount)
{
    FString Error;
    if (!InMap.Validate(Error) || PlayerCount < 2 || PlayerCount > 4) return false;
    *this = FTycoonMatch(); Map = InMap;
    Owners.Init(INDEX_NONE, Map.Tiles.Num());
    for (int32 I = 0; I < PlayerCount; ++I)
    { FTycoonPlayer P; P.Name = FString::Printf(TEXT("Player %d"), I + 1); P.Cash = Map.StartCash; Players.Add(P); }
    Message = TEXT("Roll to begin. Buy empty lots and collect rent.");
    return true;
}

bool FTycoonMatch::Roll(int32 Actor, int32 Die, int32 EventCash)
{
    if (Actor != Turn || !Players.IsValidIndex(Actor) || Phase != ETycoonPhase::Roll || Die < 1 || Die > 6 || (EventCash != 500 && EventCash != -400)) return false;
    auto& P = Players[Turn]; LastDie = Die;
    if (P.Position + Die >= Map.Tiles.Num()) P.Cash += Map.Salary;
    P.Position = (P.Position + Die) % Map.Tiles.Num();
    const auto& T = Map.Tiles[P.Position]; Phase = ETycoonPhase::End;
    Message = FString::Printf(TEXT("%s rolled %d: %s"), *P.Name, Die, *T.Name);
    if (T.Type == ETycoonTile::Property)
    {
        const int32 Owner = Owners[P.Position];
        if (Owner == INDEX_NONE && P.Cash >= T.Price) Phase = ETycoonPhase::Purchase;
        else if (Players.IsValidIndex(Owner) && Owner != Turn)
        {
            // Insolvent players pay only their available cash; no money is minted.
            const int32 Paid = FMath::Min(P.Cash, T.Rent);
            P.Cash -= T.Rent; Players[Owner].Cash += Paid;
            Message += FString::Printf(TEXT(" | rent %d"), Paid);
        }
    }
    else if (T.Type == ETycoonTile::Tax) P.Cash -= T.Rent;
    else if (T.Type == ETycoonTile::Event) { P.Cash += EventCash; Message += FString::Printf(TEXT(" | event %+d"), EventCash); }
    SettleBankruptcy(); return true;
}

bool FTycoonMatch::Buy(int32 Actor)
{
    if (Actor != Turn || !Players.IsValidIndex(Actor) || Phase != ETycoonPhase::Purchase) return false;
    auto& P = Players[Turn]; const auto& T = Map.Tiles[P.Position];
    if (Owners[P.Position] != INDEX_NONE || P.Cash < T.Price) return false;
    Owners[P.Position] = Turn; P.Cash -= T.Price; Phase = ETycoonPhase::End;
    Message = FString::Printf(TEXT("%s bought %s"), *P.Name, *T.Name); return true;
}

bool FTycoonMatch::EndTurn(int32 Actor)
{
    if (Actor != Turn || !Players.IsValidIndex(Actor) || (Phase != ETycoonPhase::Purchase && Phase != ETycoonPhase::End)) return false;
    do { Turn = (Turn + 1) % Players.Num(); if (Turn == 0) ++Round; } while (Players[Turn].bBankrupt);
    Phase = ETycoonPhase::Roll; return true;
}

void FTycoonMatch::SettleBankruptcy()
{
    auto& P = Players[Turn];
    if (P.Cash >= 0) return;
    P.Cash = 0; P.bBankrupt = true;
    for (int32& Owner : Owners) if (Owner == Turn) Owner = INDEX_NONE;
    int32 Alive = 0;
    for (int32 I = 0; I < Players.Num(); ++I) if (!Players[I].bBankrupt) { ++Alive; Winner = I; }
    if (Alive == 1) { Phase = ETycoonPhase::Finished; Message = Players[Winner].Name + TEXT(" wins!"); }
    else Winner = INDEX_NONE;
}

bool FTycoonMatch::BotShouldBuy(ETycoonDifficulty Difficulty) const
{
    if (Phase != ETycoonPhase::Purchase || !Players.IsValidIndex(Turn)) return false;
    const auto& P = Players[Turn];
    const int32 Remaining = P.Cash - Map.Tiles[P.Position].Price;
    if (Remaining < 0) return false;
    if (Difficulty == ETycoonDifficulty::Easy) return Remaining >= 2500;
    if (Difficulty == ETycoonDifficulty::Normal) return Remaining >= 1500;
    int32 MaxRent = 0;
    for (int32 I = 0; I < Owners.Num(); ++I)
        if (Owners[I] != INDEX_NONE && Owners[I] != Turn) MaxRent = FMath::Max(MaxRent, Map.Tiles[I].Rent);
    return Remaining >= FMath::Max(800, MaxRent * 2);
}
