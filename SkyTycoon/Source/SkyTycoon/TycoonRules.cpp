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
            static_cast<uint8>(T.Type) > static_cast<uint8>(ETycoonTile::Inn))
        { Error = TEXT("Invalid tile, duplicate cell, or start is not tile zero."); return false; }
        Cells.Add(T.Cell);
        const auto& Next = Tiles[(I + 1) % Tiles.Num()];
        if (FMath::Abs(T.Cell.X - Next.Cell.X) + FMath::Abs(T.Cell.Y - Next.Cell.Y) != 1)
        { Error = TEXT("Tiles must form a closed adjacent path."); return false; }
    }
    // Old/custom maps without facilities remain valid: detention stays at the current tile.
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
        if (I == 4 || I == 16) { T.Type = ETycoonTile::Shop; T.Name = TEXT("SHOP"); }
        if (I == 6) { T.Type = ETycoonTile::Jail; T.Name = TEXT("JAIL"); }
        if (I == 12) { T.Type = ETycoonTile::Hospital; T.Name = TEXT("HOSPITAL"); }
        if (I == 20) { T.Type = ETycoonTile::GoToJail; T.Name = TEXT("ARREST"); }
        if (I == 18) { T.Type = ETycoonTile::Inn; T.Name = TEXT("INN"); }
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
    Traps.Init(ETycoonTrap::None, Map.Tiles.Num());
    for (int32 I = 0; I < PlayerCount; ++I)
    { FTycoonPlayer P; P.Name = FString::Printf(TEXT("Player %d"), I + 1); P.Cash = Map.StartCash; P.Inventory.Init(0,static_cast<int32>(ETycoonItem::Count)); Players.Add(P); }
    Message = TEXT("Roll to begin. Buy empty lots and collect rent.");
    return true;
}

bool FTycoonMatch::IsActor(int32 Actor) const
{
    return Actor == Turn && Players.IsValidIndex(Actor) && !Players[Actor].bBankrupt && Phase != ETycoonPhase::Finished;
}

bool FTycoonMatch::Roll(int32 Actor, int32 Die, int32 EventCash, int32 SecondDie, int32 ThirdDie)
{
    if (!IsActor(Actor) || Phase != ETycoonPhase::Roll || Die < 1 || Die > 6 || SecondDie < 0 || SecondDie > 6 || ThirdDie < 0 || ThirdDie > 6 || (ThirdDie > 0 && SecondDie == 0) || (EventCash != 500 && EventCash != -400)) return false;
    auto& P = Players[Turn];
    const int32 DiceCount = 1 + (SecondDie > 0 ? 1 : 0) + (ThirdDie > 0 ? 1 : 0);
    if (DiceCount > P.MaxDice()) return false;
    if (P.Stay != ETycoonStay::None) return false;
    const int32 Steps = P.ForcedDie > 0 ? P.ForcedDie : Die + SecondDie + ThirdDie;
    P.ForcedDie = 0;
    if (P.CarRolls > 0) --P.CarRolls;
    LastDie = Steps; Phase = ETycoonPhase::End;
    for (int32 Step = 0; Step < Steps; ++Step)
    {
        P.Position = (P.Position + 1) % Map.Tiles.Num();
        if (P.Position == 0) P.Cash += Map.Salary;
        const ETycoonTrap Trap = Traps[P.Position];
        if (Trap == ETycoonTrap::None) continue;
        Traps[P.Position] = ETycoonTrap::None;
        if (Trap == ETycoonTrap::Roadblock) break;
        if (P.bShield) { P.bShield = false; continue; }
        P.Cash -= FTycoonCatalog::Treatment;
        SendTo(ETycoonStay::Hospital,2);
        Message = P.Name + TEXT(" hit a bomb: hospital 2 turns, fee $600.");
        SettleBankruptcy(); return true;
    }
    ResolveTile(EventCash);
    return true;
}

void FTycoonMatch::ResolveTile(int32 EventCash)
{
    auto& P = Players[Turn];
    const auto& T = Map.Tiles[P.Position]; Phase = ETycoonPhase::End;
    Message = FString::Printf(TEXT("%s moved %d: %s"), *P.Name, LastDie, *T.Name);
    if (T.Type == ETycoonTile::Property)
    {
        const int32 Owner = Owners[P.Position];
        if (Owner == INDEX_NONE && P.Cash >= T.Price) Phase = ETycoonPhase::Purchase;
        else if (Players.IsValidIndex(Owner) && Owner != Turn)
        {
            if (P.bRentPass) { P.bRentPass = false; Message += TEXT(" | rent waived"); return; }
            // Insolvent players pay only their available cash; no money is minted.
            const int32 Paid = FMath::Min(P.Cash, T.Rent);
            P.Cash -= T.Rent; Players[Owner].Cash += Paid;
            Message += FString::Printf(TEXT(" | rent %d"), Paid);
        }
    }
    else if (T.Type == ETycoonTile::Tax) P.Cash -= T.Rent;
    else if (T.Type == ETycoonTile::Event) { P.Cash += EventCash; Message += FString::Printf(TEXT(" | event %+d"), EventCash); }
    else if (T.Type == ETycoonTile::Shop) { Phase = ETycoonPhase::Shopping; bShopped = false; Message += TEXT(" | choose an item or leave"); }
    else if (T.Type == ETycoonTile::Inn) { Phase = ETycoonPhase::Lodging; Message += TEXT(" | stay $300: rest 1 turn + shield, or leave"); }
    else if (T.Type == ETycoonTile::GoToJail) { SendTo(ETycoonStay::Jail,2); Message = P.Name + TEXT(" arrested: jail for 2 turns."); }
    SettleBankruptcy();
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
    if (!IsActor(Actor) || (Phase != ETycoonPhase::Purchase && Phase != ETycoonPhase::End && Phase != ETycoonPhase::Shopping && Phase != ETycoonPhase::Lodging)) return false;
    do { Turn = (Turn + 1) % Players.Num(); if (Turn == 0) ++Round; } while (Players[Turn].bBankrupt);
    bUsedItem = false; bShopped = false;
    Phase = Players[Turn].Stay != ETycoonStay::None ? ETycoonPhase::Detained : ETycoonPhase::Roll;
    return true;
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
