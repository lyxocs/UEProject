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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarlockSmoothingTest,"Warlock.Network.SnapshotInterpolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWarlockSmoothingTest::RunTest(const FString&)
{
    FWarlockNetworkSmoothing Buffer;Buffer.Reset(0,FVector::ZeroVector,FQuat::Identity);
    Buffer.Push(.03,FVector(30,0,0),FRotator(0,170,0).Quaternion());
    Buffer.Push(.06,FVector(60,0,0),FRotator(0,-170,0).Quaternion());
    auto Mid=Buffer.Evaluate(.12);
    TestTrue(TEXT("Render between received positions"),Mid.GetLocation().Equals(FVector(45,0,0),.001));
    TestTrue(TEXT("Yaw takes shortest path through 180 degrees"),FMath::IsNearlyEqual(FMath::Abs(Mid.Rotator().Yaw),180.f,.01f));
    TestTrue(TEXT("Packet outage never extrapolates into lava"),Buffer.Evaluate(10).GetLocation().Equals(FVector(60,0,0)));
    Buffer.Push(10,FVector(1000,0,0),FQuat::Identity,true);
    TestTrue(TEXT("Teleport snaps without sweeping across arena"),Buffer.Evaluate(10).GetLocation().Equals(FVector(1000,0,0)));
    for(int I=1;I<100;++I)Buffer.Push(10+I*.03,FVector(1000+I,0,0),FQuat::Identity);
    TestTrue(TEXT("History remains bounded"),Buffer.Samples.Num()<=12);
    FWarlockNetworkSmoothing Jitter;Jitter.Reset(0,FVector::ZeroVector,FQuat::Identity);
    Jitter.Push(.025,FVector(5,0,0),FQuat::Identity);Jitter.Push(.07,FVector(12,0,0),FQuat::Identity);Jitter.Push(.09,FVector(18,0,0),FQuat::Identity);
    double Previous=0;bool Monotonic=true;
    for(int I=0;I<40;++I){double X=Jitter.Evaluate(I/120.).GetLocation().X;Monotonic&=X>=Previous-.001&&X<=18.001;Previous=X;}
    TestTrue(TEXT("Jitter stays monotonic and bounded"),Monotonic);return true;
}
#endif
