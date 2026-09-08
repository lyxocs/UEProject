#include "TycoonRules.h"

namespace
{
    constexpr int32 Prices[] = { 1200, 600, 1800, 800, 700, 700, 900, 500, 800 };
    const TCHAR* Names[] = { TEXT("Bomb"), TEXT("Roadblock"), TEXT("Car"), TEXT("Shield"), TEXT("Remote dice"), TEXT("Medkit"), TEXT("Jail pass"), TEXT("Sweeper"), TEXT("Rent pass") };
    const TCHAR* Hints[] = {
        TEXT("Select an empty road within 6 steps. Hit: $600 + hospital 2 turns."),
        TEXT("Select an empty road within 6 steps. Next traveller must stop."),
        TEXT("Up to 3 dice for your next 3 moves. Traps still apply."),
        TEXT("Block one bomb. Does not block roadblocks or arrest."),
        TEXT("Choose 1-6 steps for your next move, overriding car dice."),
        TEXT("Leave hospital now; roll this turn. No fee refund."),
        TEXT("Leave jail now; roll this turn."),
        TEXT("Remove all traps in the next 6 steps."),
        TEXT("Waive the next rent payment. Remains until triggered.")
    };
    bool ValidItem(ETycoonItem I) { return static_cast<uint8>(I) < static_cast<uint8>(ETycoonItem::Count); }
}
int32 FTycoonCatalog::Price(ETycoonItem I) { return ValidItem(I) ? Prices[static_cast<int32>(I)] : -1; }
FString FTycoonCatalog::Name(ETycoonItem I) { return ValidItem(I) ? Names[static_cast<int32>(I)] : TEXT("Invalid item"); }
FString FTycoonCatalog::Help(ETycoonItem I) { return ValidItem(I) ? Hints[static_cast<int32>(I)] : TEXT(""); }
int32 FTycoonPlayer::ItemCount(ETycoonItem I) const { return Inventory.IsValidIndex(static_cast<int32>(I)) ? Inventory[static_cast<int32>(I)] : 0; }
int32 FTycoonPlayer::InventorySize() const { int32 N=0; for (int32 Count : Inventory) N+=Count; return N; }

bool FTycoonMatch::BuyItem(int32 Actor, ETycoonItem Item)
{
    if (!IsActor(Actor) || Phase != ETycoonPhase::Shopping || bShopped || !ValidItem(Item)) return false;
    auto& P = Players[Turn]; const int32 Cost = FTycoonCatalog::Price(Item);
    if (P.Cash < Cost || P.InventorySize() >= FTycoonCatalog::Capacity) return false;
    P.Cash -= Cost; ++P.Inventory[static_cast<int32>(Item)]; bShopped = true;
    Message = P.Name + TEXT(" bought ") + FTycoonCatalog::Name(Item);
    return true;
}

bool FTycoonMatch::CanPlaceTrap(int32 Target) const
{
    if (!Map.Tiles.IsValidIndex(Target) || Traps[Target] != ETycoonTrap::None) return false;
    const int32 Distance = (Target - Players[Turn].Position + Map.Tiles.Num()) % Map.Tiles.Num();
    const ETycoonTile Type = Map.Tiles[Target].Type;
    if (Distance < 1 || Distance > 6 || (Type != ETycoonTile::Property && Type != ETycoonTile::Tax && Type != ETycoonTile::Event && Type != ETycoonTile::Rest)) return false;
    for (const auto& P : Players) if (!P.bBankrupt && P.Position == Target) return false;
    return true;
}

bool FTycoonMatch::UseItem(int32 Actor, ETycoonItem Item, int32 Target)
{
    if (!IsActor(Actor) || !ValidItem(Item) || bUsedItem || (Phase != ETycoonPhase::Roll && Phase != ETycoonPhase::Detained)) return false;
    auto& P = Players[Turn];
    if (P.ItemCount(Item) < 1) return false;
    if (Phase == ETycoonPhase::Detained && Item != ETycoonItem::Medkit && Item != ETycoonItem::JailPass) return false;
    switch (Item)
    {
    case ETycoonItem::Bomb:
    case ETycoonItem::Roadblock:
        if (!CanPlaceTrap(Target)) return false;
        Traps[Target] = Item == ETycoonItem::Bomb ? ETycoonTrap::Bomb : ETycoonTrap::Roadblock;
        break;
    case ETycoonItem::Car:
        if (P.CarRolls > 0) return false;
        P.CarRolls = 3; break;
    case ETycoonItem::Shield:
        if (P.bShield) return false;
        P.bShield = true; break;
    case ETycoonItem::RemoteDice:
        if (Target < 1 || Target > 6 || P.ForcedDie > 0) return false;
        P.ForcedDie = Target; break;
    case ETycoonItem::Medkit:
        if (P.Stay != ETycoonStay::Hospital) return false;
        P.Stay = ETycoonStay::None; P.StayTurns = 0; Phase = ETycoonPhase::Roll; break;
    case ETycoonItem::JailPass:
        if (P.Stay != ETycoonStay::Jail) return false;
        P.Stay = ETycoonStay::None; P.StayTurns = 0; Phase = ETycoonPhase::Roll; break;
    case ETycoonItem::Sweeper:
    {
        bool Removed = false;
        for (int32 Step=1; Step<=6; ++Step)
        {
            auto& Trap = Traps[(P.Position+Step)%Traps.Num()];
            if (Trap != ETycoonTrap::None) { Trap = ETycoonTrap::None; Removed = true; }
        }
        if (!Removed) return false;
        break;
    }
    case ETycoonItem::RentPass:
        if (P.bRentPass) return false;
        P.bRentPass = true; break;
    default: return false;
    }
    --P.Inventory[static_cast<int32>(Item)]; bUsedItem = true;
    Message = P.Name + TEXT(" used ") + FTycoonCatalog::Name(Item); return true;
}

void FTycoonMatch::SendTo(ETycoonStay Stay, int32 Turns)
{
    auto& P = Players[Turn]; P.Stay = Stay; P.StayTurns = Turns; P.CarRolls = 0;
    P.ForcedDie = 0;
    const ETycoonTile Facility = Stay == ETycoonStay::Jail ? ETycoonTile::Jail : ETycoonTile::Hospital;
    for (int32 I=0; I<Map.Tiles.Num(); ++I) if (Map.Tiles[I].Type == Facility) { P.Position=I; break; }
    // Arrival's turn ends; only future detained turns count down. Teleport grants no salary.
    Phase = ETycoonPhase::End;
}

bool FTycoonMatch::Recover(int32 Actor, bool bPay)
{
    if (!IsActor(Actor) || Phase != ETycoonPhase::Detained) return false;
    auto& P = Players[Turn];
    const int32 Fee = P.Stay == ETycoonStay::Jail ? FTycoonCatalog::Bail : FTycoonCatalog::Treatment;
    if (bPay)
    {
        if (P.Stay == ETycoonStay::Inn) return false;
        if (P.Cash < Fee) return false;
        P.Cash -= Fee; P.StayTurns = 0; P.Stay = ETycoonStay::None; Phase = ETycoonPhase::Roll;
        Message = P.Name + TEXT(" paid for release. Roll now."); return true;
    }
    --P.StayTurns;
    if (P.StayTurns <= 0) { P.StayTurns = 0; P.Stay = ETycoonStay::None; }
    Message = P.Name + TEXT(" waits for recovery/release.");
    Phase = ETycoonPhase::End;
    return EndTurn(Actor);
}

bool FTycoonMatch::BotUseItem()
{
    if (!IsActor(Turn) || !Players[Turn].bBot || bUsedItem) return false;
    if (UseItem(Turn, ETycoonItem::Medkit) || UseItem(Turn, ETycoonItem::JailPass)) return true;
    if (Phase != ETycoonPhase::Roll) return false;
    if (UseItem(Turn, ETycoonItem::Sweeper) || UseItem(Turn, ETycoonItem::Shield) || UseItem(Turn, ETycoonItem::RentPass)) return true;
    for (int32 Step=1; Step<=6; ++Step)
    {
        const int32 Target = (Players[Turn].Position+Step)%Map.Tiles.Num();
        if (UseItem(Turn, ETycoonItem::Bomb,Target) || UseItem(Turn, ETycoonItem::Roadblock,Target)) return true;
    }
    // Seek a shop or affordable vacant property using public board state.
    for (int32 Step=1; Step<=6; ++Step)
    {
        const int32 Target=(Players[Turn].Position+Step)%Map.Tiles.Num();
        bool Safe = true;
        for (int32 J=1; J<=Step; ++J) if (Traps[(Players[Turn].Position+J)%Traps.Num()] != ETycoonTrap::None) Safe=false;
        if (Safe && (Map.Tiles[Target].Type==ETycoonTile::Shop || (Map.Tiles[Target].Type==ETycoonTile::Property && Owners[Target]==INDEX_NONE && Players[Turn].Cash>=Map.Tiles[Target].Price)))
            if (UseItem(Turn,ETycoonItem::RemoteDice,Step)) return true;
    }
    return UseItem(Turn,ETycoonItem::Car);
}

bool FTycoonMatch::CheckIn(int32 Actor)
{
    if (!IsActor(Actor) || Phase != ETycoonPhase::Lodging || Players[Turn].Cash < FTycoonCatalog::InnFee) return false;
    auto& P = Players[Turn];
    P.Cash -= FTycoonCatalog::InnFee; P.Stay = ETycoonStay::Inn; P.StayTurns = 1; P.bShield = true;
    Phase = ETycoonPhase::End;
    Message = P.Name + TEXT(" checked in: rest 1 turn, shield ready.");
    return true;
}

bool FTycoonMatch::BotShop(ETycoonDifficulty Difficulty)
{
    if (!IsActor(Turn) || !Players[Turn].bBot || Phase != ETycoonPhase::Shopping) return false;
    const int32 Reserve = Difficulty == ETycoonDifficulty::Easy ? 2500 : Difficulty == ETycoonDifficulty::Normal ? 1500 : 800;
    const ETycoonItem Choices[] = { ETycoonItem::Medkit, ETycoonItem::Shield, ETycoonItem::JailPass, ETycoonItem::Car, ETycoonItem::Bomb, ETycoonItem::Roadblock, ETycoonItem::RemoteDice, ETycoonItem::Sweeper, ETycoonItem::RentPass };
    for (auto Item : Choices)
        if (Players[Turn].ItemCount(Item)==0 && Players[Turn].Cash-FTycoonCatalog::Price(Item)>=Reserve && BuyItem(Turn,Item)) return true;
    return false;
}
