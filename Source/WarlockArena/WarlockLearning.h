#pragma once
#include "CoreMinimal.h"

class AWarlockAI;
class AWarlockPawn;
struct FWarlockDifficulty
{
    float ReactionMin,ReactionMax,Interval,AimError,PolicyChance,SkipChance;
    static FWarlockDifficulty Get(int Tier);
    static const TCHAR* Name(int Tier);
};

// Small masked softmax actor + linear TD critic. Runs without Python/GPU services.
struct FWarlockPolicy
{
    static constexpr int Features=20,Actions=11;
    float Actor[Actions][Features]={};
    float Critic[Features]={};
    int Updates=0;
    bool bReady=false;
    static FWarlockPolicy& Shared();
    bool Load(const FString& Path);
    bool Save(const FString& Path) const;
    int Choose(AWarlockAI* AI,float* X,float* Probabilities);
    float Value(const float* X) const;
};

struct FWarlockExperience
{
    float X[FWarlockPolicy::Features]={},Probabilities[FWarlockPolicy::Actions]={};
    float PreviousScore=0;
    int Action=-1;
};
