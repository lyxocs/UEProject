#include "WarlockGame.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarlockSweepTest,"Warlock.Rules.SweptContacts",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWarlockSweepTest::RunTest(const FString&)
{
    float Time=0;
    TestTrue(TEXT("High-speed shot cannot pass through a player"),WarlockRules::SweepCircle(FVector::ZeroVector,FVector(1000,0,0),FVector(500,0,0),55,Time));
    TestTrue(TEXT("Contact at near surface rather than center"),FMath::IsNearlyEqual(Time,.445f));
    TestFalse(TEXT("Zero-length miss is safe"),WarlockRules::SweepCircle(FVector::ZeroVector,FVector::ZeroVector,FVector(500,0,0),55,Time));
    TestFalse(TEXT("No imaginary square root for a miss"),WarlockRules::SweepCircle(FVector::ZeroVector,FVector(1000,0,0),FVector(500,100,0),55,Time));
    TestTrue(TEXT("Opposing projectiles are swept in relative space"),WarlockRules::SweepCircle(FVector(-600,0,0),FVector(0,0,0),FVector::ZeroVector,50,Time));
    TestTrue(TEXT("Collision precedes passing each other"),FMath::IsNearlyEqual(Time,550.f/600));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarlockImpulseTest,"Warlock.Rules.ImpulseUsesPreviousK",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWarlockImpulseTest::RunTest(const FString&)
{
    TestEqual(TEXT("7 damage K0 -> 21 units per step"),WarlockRules::Impulse(7,0),21.f);
    TestEqual(TEXT("7 damage K50 -> 31.5"),WarlockRules::Impulse(7,50),31.5f);
    TestEqual(TEXT("K100 -> 42; no extra delta-time multiplication"),WarlockRules::Impulse(7,100),42.f);
    TestEqual(TEXT("Target/spell factors multiply impulse"),WarlockRules::Impulse(7,100,.5f),21.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarlockProgressionTest,"Warlock.Rules.PricesAndCooldowns",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWarlockProgressionTest::RunTest(const FString&)
{
    TestEqual(TEXT("Blink starts at 16s"),WarlockRules::Cooldown(3,1,false),16.f);
    TestEqual(TEXT("Lightning L7"),WarlockRules::Cooldown(2,7,false),13.f);
    TestEqual(TEXT("Chain L7 is allowed by map script"),WarlockRules::LevelCap(7),7);
    TestEqual(TEXT("Chain upgrade 1->2"),WarlockRules::UpgradeCost(7,1),4);
    TestEqual(TEXT("Fireball upgrade starts at 3"),WarlockRules::UpgradeCost(0,1),3);
    TestEqual(TEXT("Spray learning"),WarlockRules::LearnCost(4),14);
    return true;
}
#endif
