#include "WarlockGame.h"
#include "Net/UnrealNetwork.h"

namespace WarlockRules
{
FString SkillName(int Slot)
{
    static const TCHAR* Names[] = {TEXT("Fireball"),TEXT("Smite"),TEXT("Lightning"),TEXT("Blink"),TEXT("Fire spray"),TEXT("Meteor"),TEXT("Reflect"),TEXT("Chain")};
    return Slot >= 0 && Slot < Slots ? Names[Slot] : TEXT("Unknown");
}
float Cooldown(int Slot, int Level, bool Alternate)
{
    const int I=FMath::Clamp(Level-1,0,8);
    const float Lightning[]={16.5f,15.5f,15,14.5f,14,13.5f,13,12.5f,12};
    const float Blink[]={16,13.5f,11.5f,10,9,8,7,6,5};
    const float Fold[]={18,15,12.5f,10.5f,9,8,7,6,5};
    const float Reflect[]={25,22.5f,20.5f,19,18,17,16,15,14};
    switch(Slot)
    {
    case 0: return 4.8f;
    case 1: return 3.f;
    case 2: return Lightning[I];
    case 3: return Alternate?Fold[I]:Blink[I];
    case 4: return Alternate?16.5f-I:15.5f-.75f*I;
    case 5: return 20.f-.5f*I;
    case 6: return Reflect[I];
    case 7: return Alternate?16.f-I:I==8?8.f:17.f-2.f*I;
    default: return 0;
    }
}
int LearnCost(int Slot) { return Slot==4||Slot==5 ? 14 : Slot==6 ? 12 : 11; }
int UpgradeCost(int Slot,int Level) { return Slot==0 ? 2+Level : (Slot==2?8:Slot==7?4:6)+Level-1; }
int LevelCap(int Slot) { return Slot==0?10:Slot==4||Slot==5||Slot==6?6:7; }
float Impulse(float Damage,float PreviousKnockback,float Multiplier) {return (100.f+PreviousKnockback)*Damage*Step*Multiplier;}
bool SweepCircle(FVector Start,FVector End,FVector Center,float HitRadius,float& Time)
{
    FVector P=Start-Center,V=End-Start;P.Z=V.Z=0;
    double C=P.SizeSquared()-FMath::Square(HitRadius);if(C<=0){Time=0;return true;}
    double A=V.SizeSquared();if(A<UE_SMALL_NUMBER)return false;
    double B=FVector::DotProduct(P,V),D=B*B-A*C;if(D<0)return false;
    double T=(-B-FMath::Sqrt(D))/A;if(T<0||T>1)return false;Time=float(T);return true;
}
}
AWarlockState::AWarlockState()
{
    Levels.Init(0,8); Levels[0]=Levels[1]=1; Modes.Init(false,8); Items.Init(0,8);
    for(int I=0;I<8;++I)Families.Add(I);
}
int AWarlockState::Level(int Slot) const { return Levels.IsValidIndex(Slot)?Levels[Slot]:0; }
int AWarlockState::LearnedCount() const { int N=0; for(int I=2;I<8;++I) N+=Level(I)>0; return N; }
void AWarlockState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWarlockState,Gold); DOREPLIFETIME(AWarlockState,Points); DOREPLIFETIME(AWarlockState,Team);
    DOREPLIFETIME(AWarlockState,Levels); DOREPLIFETIME(AWarlockState,Modes); DOREPLIFETIME(AWarlockState,Items);
    DOREPLIFETIME(AWarlockState,DefenseChoice);
    DOREPLIFETIME(AWarlockState,RoundKills);DOREPLIFETIME(AWarlockState,RoundAssists);DOREPLIFETIME(AWarlockState,KillIncome);DOREPLIFETIME(AWarlockState,AssistIncome);DOREPLIFETIME(AWarlockState,VictoryIncome);DOREPLIFETIME(AWarlockState,DamageIncome);DOREPLIFETIME(AWarlockState,SupplyIncome);DOREPLIFETIME(AWarlockState,RoundPlacement);DOREPLIFETIME(AWarlockState,RoundDamage);
    DOREPLIFETIME(AWarlockState,Families);
    DOREPLIFETIME(AWarlockState,LifeMastery); DOREPLIFETIME(AWarlockState,RangeMastery); DOREPLIFETIME(AWarlockState,TimeMastery);
    DOREPLIFETIME(AWarlockState,StoneSlot); DOREPLIFETIME(AWarlockState,bHasStone); DOREPLIFETIME(AWarlockState,bBot);
    DOREPLIFETIME(AWarlockState,Appearance);DOREPLIFETIME(AWarlockState,ColorIndex);
    DOREPLIFETIME(AWarlockState,bReady);
}
void AWarlockMatchState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AWarlockMatchState,Round); DOREPLIFETIME(AWarlockMatchState,Phase);
    DOREPLIFETIME(AWarlockMatchState,Remaining); DOREPLIFETIME(AWarlockMatchState,ArenaRadius);
    DOREPLIFETIME(AWarlockMatchState,bTeams); DOREPLIFETIME(AWarlockMatchState,Announcement);
    DOREPLIFETIME(AWarlockMatchState,ParticipantCount); DOREPLIFETIME(AWarlockMatchState,bOnlineRoom); DOREPLIFETIME(AWarlockMatchState,RoomHostId);
    DOREPLIFETIME(AWarlockMatchState,AIDifficulty);
    DOREPLIFETIME(AWarlockMatchState,EconomySettings);
    DOREPLIFETIME(AWarlockMatchState,bFirstBlood);
}
