#include "TycoonRules.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTycoonRulesTest,"SkyTycoon.Rules.TurnAndEconomy",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTycoonRulesTest::RunTest(const FString& Parameters)
{
    FTycoonMatch S; TestTrue(TEXT("Start"),S.Start(FTycoonMap::Default(),2));
    TestFalse(TEXT("Reject wrong player"),S.Roll(1,1,500));
    TestFalse(TEXT("Reject dice 7"),S.Roll(0,7,500));
    TestTrue(TEXT("Move to property"),S.Roll(0,1,500));
    TestTrue(TEXT("Normal bot buys affordable land"),S.BotShouldBuy(ETycoonDifficulty::Normal));
    S.Players[0].Cash = S.Map.Tiles[1].Price + 1000;
    TestFalse(TEXT("Normal bot keeps reserve"),S.BotShouldBuy(ETycoonDifficulty::Normal));
    TestTrue(TEXT("Hard bot can invest with lower exposure"),S.BotShouldBuy(ETycoonDifficulty::Hard));
    S.Players[0].Cash = S.Map.StartCash;
    TestTrue(TEXT("Buy"),S.Buy(0));
    TestFalse(TEXT("Cannot buy twice"),S.Buy(0));
    const int32 Cash = S.Players[0].Cash;
    TestTrue(TEXT("Next player"),S.EndTurn(0));
    TestTrue(TEXT("Rent landing"),S.Roll(1,1,500));
    TestEqual(TEXT("Owner receives rent"),S.Players[0].Cash,Cash+S.Map.Tiles[1].Rent);
    S.EndTurn(1); S.Players[0].Position=23;
    S.Roll(0,1,500); TestEqual(TEXT("Salary on wrap"),S.Players[0].Cash,Cash+S.Map.Tiles[1].Rent+S.Map.Salary);
    S.EndTurn(0); S.Players[1].Position=0; S.Players[1].Cash=1;
    S.Roll(1,1,500); TestTrue(TEXT("Insolvency finishes duel"),S.Phase==ETycoonPhase::Finished);
    TestEqual(TEXT("Winner"),S.Winner,0);
    auto Bad = FTycoonMap::Default(); Bad.Tiles[1].Cell=Bad.Tiles[0].Cell;
    FString Error; TestFalse(TEXT("Reject overlapping map"),Bad.Validate(Error));
    TestFalse(TEXT("Reject five seats"),S.Start(FTycoonMap::Default(),5));
    TestTrue(TEXT("Allow four seats"),S.Start(FTycoonMap::Default(),4));
    return true;
}
#endif
