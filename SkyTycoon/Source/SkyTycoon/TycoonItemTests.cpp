#include "TycoonRules.h"
#include "TycoonGame.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    void Give(FTycoonMatch& S,ETycoonItem I,int32 Count=1) { S.Players[S.Turn].Inventory[static_cast<int32>(I)]=Count; }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTycoonShopTest,"SkyTycoon.Rules.ShopAndInventory",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTycoonShopTest::RunTest(const FString& Parameters)
{
    FTycoonMatch S; S.Start(FTycoonMap::Default(),2);
    TestFalse(TEXT("No remote shopping"),S.BuyItem(0,ETycoonItem::Bomb));
    S.Roll(0,4,500);
    TestTrue(TEXT("Shop tile opens shop"),S.Phase==ETycoonPhase::Shopping);
    TestFalse(TEXT("Wrong player rejected"),S.BuyItem(1,ETycoonItem::Bomb));
    TestFalse(TEXT("Invalid item rejected"),S.BuyItem(0,ETycoonItem::Count));
    S.Players[0].Cash=1199;
    TestFalse(TEXT("Insufficient cash"),S.BuyItem(0,ETycoonItem::Bomb));
    TestEqual(TEXT("Failed purchase atomic"),S.Players[0].Cash,1199);
    S.Players[0].Cash=1200;
    TestTrue(TEXT("Exact price accepted"),S.BuyItem(0,ETycoonItem::Bomb));
    TestEqual(TEXT("Inventory credited"),S.Players[0].ItemCount(ETycoonItem::Bomb),1);
    TestFalse(TEXT("One purchase per visit"),S.BuyItem(0,ETycoonItem::Roadblock));
    TestFalse(TEXT("Cannot use after landing"),S.UseItem(0,ETycoonItem::Bomb,5));
    S.bShopped=false; S.Players[0].Cash=10000; Give(S,ETycoonItem::Bomb,8);
    TestFalse(TEXT("Bag capacity enforced"),S.BuyItem(0,ETycoonItem::Car));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTycoonTrapTest,"SkyTycoon.Rules.TrapsAndMovement",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTycoonTrapTest::RunTest(const FString& Parameters)
{
    FTycoonMatch S; S.Start(FTycoonMap::Default(),2); Give(S,ETycoonItem::Bomb); Give(S,ETycoonItem::Shield);
    TestFalse(TEXT("Trap range"),S.UseItem(0,ETycoonItem::Bomb,7));
    TestFalse(TEXT("Facility trap rejected"),S.UseItem(0,ETycoonItem::Bomb,4));
    TestFalse(TEXT("Negative target rejected"),S.UseItem(0,ETycoonItem::Bomb,-1));
    TestEqual(TEXT("Failed uses keep stock"),S.Players[0].ItemCount(ETycoonItem::Bomb),1);
    TestTrue(TEXT("Place bomb"),S.UseItem(0,ETycoonItem::Bomb,2));
    TestFalse(TEXT("One item each turn"),S.UseItem(0,ETycoonItem::Shield));
    S.Roll(0,6,500);
    TestEqual(TEXT("Passing bomb stops and teleports"),S.Players[0].Position,12);
    TestEqual(TEXT("Bomb damage"),S.Players[0].Cash,11400);
    TestEqual(TEXT("Two future turns"),S.Players[0].StayTurns,2);
    TestTrue(TEXT("Trap consumed"),S.Traps[2]==ETycoonTrap::None);
    TestTrue(TEXT("Arrival does not consume recovery turn"),S.Phase==ETycoonPhase::End);
    S.Start(FTycoonMap::Default(),2); S.Traps[2]=ETycoonTrap::Bomb; S.Players[0].bShield=true;
    S.Roll(0,3,500);
    TestEqual(TEXT("Shield continues movement"),S.Players[0].Position,3);
    TestFalse(TEXT("Shield consumed"),S.Players[0].bShield);
    S.Start(FTycoonMap::Default(),2); S.Traps[2]=ETycoonTrap::Roadblock;
    S.Roll(0,6,500);
    TestEqual(TEXT("Roadblock stops pass-through"),S.Players[0].Position,2);
    TestTrue(TEXT("Stop resolves purchase"),S.Phase==ETycoonPhase::Purchase);
    S.Start(FTycoonMap::Default(),2); Give(S,ETycoonItem::Car); S.UseItem(0,ETycoonItem::Car);
    S.Players[0].Position=23; S.Roll(0,1,500,2);
    TestEqual(TEXT("Car two dice"),S.LastDie,3);
    TestEqual(TEXT("Wrap grants salary"),S.Players[0].Cash,13500);
    TestEqual(TEXT("Car duration used once"),S.Players[0].CarRolls,2);
    S.EndTurn(0); S.Roll(1,1,500); S.EndTurn(1);
    Give(S,ETycoonItem::RemoteDice); S.UseItem(0,ETycoonItem::RemoteDice,1); S.Roll(0,6,500,6);
    TestEqual(TEXT("Remote overrides both car dice"),S.LastDie,1);
    TestEqual(TEXT("Remote still consumes car move"),S.Players[0].CarRolls,1);
    S.Start(FTycoonMap::Default(),2); S.Players[0].Cash=1; S.Traps[1]=ETycoonTrap::Bomb; S.Roll(0,1,500);
    TestTrue(TEXT("Bomb can bankrupt"),S.Phase==ETycoonPhase::Finished);
    TestFalse(TEXT("Finished match rejects recovery"),S.Recover(0,true));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTycoonFacilityTest,"SkyTycoon.Rules.FacilitiesAndRecovery",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTycoonFacilityTest::RunTest(const FString& Parameters)
{
    FTycoonMatch S; S.Start(FTycoonMap::Default(),2); S.Players[0].Position=19;
    S.Roll(0,1,500); TestEqual(TEXT("Arrest moves to jail"),S.Players[0].Position,6);
    TestEqual(TEXT("No salary during transfer"),S.Players[0].Cash,12000);
    S.EndTurn(0); S.Roll(1,1,500); S.EndTurn(1);
    TestTrue(TEXT("Detained on next turn"),S.Phase==ETycoonPhase::Detained);
    TestFalse(TEXT("Cannot bypass stay by rolling"),S.Roll(0,1,500));
    TestFalse(TEXT("Cannot bypass stay by ending"),S.EndTurn(0));
    Give(S,ETycoonItem::Medkit); TestFalse(TEXT("Wrong release card preserved"),S.UseItem(0,ETycoonItem::Medkit));
    S.Recover(0,false); TestEqual(TEXT("One wait deducted"),S.Players[0].StayTurns,1);
    TestEqual(TEXT("Waiting advances seat"),S.Turn,1);
    S.Roll(1,1,500); S.EndTurn(1); S.Recover(0,false);
    TestTrue(TEXT("Natural release"),S.Players[0].Stay==ETycoonStay::None);
    TestEqual(TEXT("Second wait still skips turn"),S.Turn,1);
    S.Roll(1,1,500); S.EndTurn(1); TestTrue(TEXT("Roll on following turn"),S.Phase==ETycoonPhase::Roll);
    S.Start(FTycoonMap::Default(),2); S.Players[0].Stay=ETycoonStay::Hospital; S.Players[0].StayTurns=2; S.Phase=ETycoonPhase::Detained;
    Give(S,ETycoonItem::Medkit); S.UseItem(0,ETycoonItem::Medkit);
    TestTrue(TEXT("Medkit immediately restores roll"),S.Phase==ETycoonPhase::Roll);
    TestEqual(TEXT("Medkit consumed"),S.Players[0].ItemCount(ETycoonItem::Medkit),0);
    S.Start(FTycoonMap::Default(),2); S.Players[0].Stay=ETycoonStay::Jail; S.Players[0].StayTurns=2; S.Phase=ETycoonPhase::Detained; S.Players[0].Cash=799;
    TestFalse(TEXT("Bail cannot make debt"),S.Recover(0,true));
    S.Players[0].Cash=800; TestTrue(TEXT("Bail at exact cost"),S.Recover(0,true));
    TestEqual(TEXT("Bail deducted"),S.Players[0].Cash,0);
    S.Start(FTycoonMap::Default(),2); S.Players[0].Position=17; S.Roll(0,1,500);
    TestTrue(TEXT("Inn offers choice"),S.Phase==ETycoonPhase::Lodging);
    TestTrue(TEXT("Inn check in"),S.CheckIn(0)); TestEqual(TEXT("Inn fee"),S.Players[0].Cash,11700);
    TestTrue(TEXT("Inn grants shield"),S.Players[0].bShield);
    TestFalse(TEXT("No repeat check in"),S.CheckIn(0));
    S.EndTurn(0); S.Roll(1,1,500); S.EndTurn(1);
    TestFalse(TEXT("Inn cannot pay to skip chosen rest"),S.Recover(0,true)); S.Recover(0,false);
    TestTrue(TEXT("Inn leaves after one wait"),S.Players[0].Stay==ETycoonStay::None);
    S.Start(FTycoonMap::Default(),2); S.Players[0].Position=17; S.Roll(0,1,500); S.EndTurn(0);
    TestEqual(TEXT("Declining inn costs nothing"),S.Players[0].Cash,12000);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTycoonUtilityTest,"SkyTycoon.Rules.UtilityItemsAndBots",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTycoonUtilityTest::RunTest(const FString& Parameters)
{
    FTycoonMatch S; S.Start(FTycoonMap::Default(),2); Give(S,ETycoonItem::Sweeper);
    TestFalse(TEXT("Empty sweep preserves item"),S.UseItem(0,ETycoonItem::Sweeper));
    S.Traps[1]=ETycoonTrap::Bomb; S.Traps[2]=ETycoonTrap::Roadblock;
    TestTrue(TEXT("Sweep both types"),S.UseItem(0,ETycoonItem::Sweeper));
    TestTrue(TEXT("Sweep clears path"),S.Traps[1]==ETycoonTrap::None && S.Traps[2]==ETycoonTrap::None);
    S.Start(FTycoonMap::Default(),2); S.Owners[1]=1; Give(S,ETycoonItem::RentPass); S.UseItem(0,ETycoonItem::RentPass); S.Roll(0,1,500);
    TestEqual(TEXT("No rent deducted"),S.Players[0].Cash,12000); TestEqual(TEXT("No rent minted"),S.Players[1].Cash,12000);
    TestFalse(TEXT("Rent pass consumed"),S.Players[0].bRentPass);
    S.Start(FTycoonMap::Default(),2); S.Players[0].bBot=true; S.Players[0].Stay=ETycoonStay::Jail; S.Players[0].StayTurns=2; S.Phase=ETycoonPhase::Detained;
    Give(S,ETycoonItem::JailPass); TestTrue(TEXT("Bot uses correct release card"),S.BotUseItem());
    TestTrue(TEXT("Bot is released"),S.Phase==ETycoonPhase::Roll);
    S.Start(FTycoonMap::Default(),2); S.Players[0].bBot=true; S.Roll(0,4,500);
    TestTrue(TEXT("Bot shops"),S.BotShop(ETycoonDifficulty::Normal));
    TestFalse(TEXT("Bot cannot repeat shopping"),S.BotShop(ETycoonDifficulty::Normal));
    TestEqual(TEXT("Only one item"),S.Players[0].InventorySize(),1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTycoonDiceTest,"SkyTycoon.Rules.DiceLimitsAndControls",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTycoonDiceTest::RunTest(const FString& Parameters)
{
    FTycoonMatch S; S.Start(FTycoonMap::Default(),2);
    TestEqual(TEXT("Default max two"),S.Players[0].MaxDice(),2);
    TestFalse(TEXT("Zero dice rejected"),S.Roll(0,0,500));
    TestFalse(TEXT("Normal triple rejected"),S.Roll(0,1,500,1,1));
    TestFalse(TEXT("Gapped dice rejected"),S.Roll(0,1,500,0,1));
    TestTrue(TEXT("Normal double accepted"),S.Roll(0,1,500,2));
    TestEqual(TEXT("Double sum"),S.LastDie,3);
    S.Start(FTycoonMap::Default(),2); Give(S,ETycoonItem::Car); S.UseItem(0,ETycoonItem::Car);
    TestEqual(TEXT("Car max three"),S.Players[0].MaxDice(),3);
    TestTrue(TEXT("Triple accepted"),S.Roll(0,1,500,1,1)); TestEqual(TEXT("Triple sum"),S.LastDie,3);
    S.Start(FTycoonMap::Default(),2); S.Players[0].CarRolls=1;
    TestTrue(TEXT("Car may choose one"),S.Roll(0,1,500)); TestEqual(TEXT("Car expires"),S.Players[0].MaxDice(),2);
    // These control methods have no world access; restore all modified CDO state.
    auto* G=GetMutableDefault<ATycoonGameMode>();
    const auto Saved=G->Match; const int32 SavedMask=G->DiceMask; const bool SavedSpeed=G->bFastBots;
    G->Match.Start(FTycoonMap::Default(),2); G->DiceMask=3; G->bFastBots=false;
    TestEqual(TEXT("Default two illuminated"),G->SelectedDiceCount(),2);
    G->ToggleDie(0); TestEqual(TEXT("Click first off"),G->DiceMask,2);
    G->ToggleDie(1); TestEqual(TEXT("Cannot switch last off"),G->DiceMask,2);
    G->ToggleDie(2); TestEqual(TEXT("Third is locked"),G->DiceMask,2);
    G->Match.Players[0].CarRolls=3; G->ToggleDie(2); TestEqual(TEXT("Third available with car"),G->DiceMask,6);
    G->ToggleDie(0); TestEqual(TEXT("Three can be lit"),G->SelectedDiceCount(),3);
    G->ToggleBotSpeed(); TestEqual(TEXT("2x interval"),G->BotInterval(),.4f);
    G->ToggleBotSpeed(); TestEqual(TEXT("1x interval"),G->BotInterval(),.8f);
    TestEqual(TEXT("Speed does not alter cash"),G->Match.Players[0].Cash,12000);
    G->Match=Saved; G->DiceMask=SavedMask; G->bFastBots=SavedSpeed;
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTycoonBotSmokeTest,"SkyTycoon.Rules.FourBotSmoke",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTycoonBotSmokeTest::RunTest(const FString& Parameters)
{
    FTycoonMatch S; S.Start(FTycoonMap::Default(),4); FRandomStream Random(731);
    for (auto& P:S.Players) P.bBot=true;
    for (int32 Step=0;Step<500 && S.Phase!=ETycoonPhase::Finished;++Step)
    {
        bool Advanced=S.BotUseItem();
        if (!Advanced)
        {
            switch (S.Phase)
            {
            case ETycoonPhase::Roll:
                Advanced=S.Roll(S.Turn,Random.RandRange(1,6),Random.RandRange(0,1) ? 500 : -400,Random.RandRange(1,6),S.Players[S.Turn].MaxDice()==3 ? Random.RandRange(1,6) : 0); break;
            case ETycoonPhase::Purchase:
                Advanced=S.BotShouldBuy(ETycoonDifficulty::Normal) ? S.Buy(S.Turn) : S.EndTurn(S.Turn); break;
            case ETycoonPhase::Shopping:
                Advanced=S.BotShop(ETycoonDifficulty::Normal) || S.EndTurn(S.Turn); break;
            case ETycoonPhase::Detained:
                Advanced=S.Recover(S.Turn,false); break;
            case ETycoonPhase::Lodging:
                Advanced=S.CheckIn(S.Turn) || S.EndTurn(S.Turn); break;
            case ETycoonPhase::End:
                Advanced=S.EndTurn(S.Turn); break;
            default: break;
            }
        }
        if (!TestTrue(TEXT("Every bot action advances"),Advanced)) return false;
        for (const auto& P:S.Players)
        {
            TestTrue(TEXT("Bag remains bounded"),P.InventorySize()<=8);
            TestTrue(TEXT("Balances settled"),P.Cash>=0);
            TestTrue(TEXT("Position within board"),S.Map.Tiles.IsValidIndex(P.Position));
        }
    }
    TestTrue(TEXT("Match progresses multiple rounds"),S.Round>10 || S.Phase==ETycoonPhase::Finished);
    return true;
}
#endif
