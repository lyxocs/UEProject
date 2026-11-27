#pragma once
#include "CoreMinimal.h"
class AWarlockPawn;
// Separate model and delayed sensor for tier six. Legacy policies are unchanged.
struct FWarlockExpertPolicy
{
    static constexpr int Features=64,Actions=61;
    float Actor[Actions][Features]={},Critic[Features]={};
    int Updates=0;
    bool bReady=false;
    static FWarlockExpertPolicy& Shared();
    bool Load(const FString& Path);
    bool Save(const FString& Path) const;
    int Choose(int First,int Count,const float* X,const bool* Mask,const float* Prior,FRandomStream& Random,bool Explore,float* Prob) const;
    void Update(const float* X,int Action,const float* Prob,float Advantage,bool Shop=false);
    float Value(const float* X) const;
};
struct FWarlockExpertExperience
{
    float X[64]={},Prob[61]={},Score=0;
    int Action[4]={-1,-1,-1,-1};
};
struct FWarlockExpertThreat {FVector Position,Velocity;float Radius,Life;bool bArea=false;};
struct FWarlockExpertFrame
{
    float Captured=0,Ready=0,Health=1,Knockback=0,Radius=1280;
    float SelfHealth=100,SelfMaxHealth=100,SelfKnockback=0;
    FVector SelfPosition=FVector::ZeroVector,SelfVelocity=FVector::ZeroVector,SelfMomentum=FVector::ZeroVector;
    TWeakObjectPtr<AWarlockPawn> Target;
    FVector Position=FVector::ZeroVector,Velocity=FVector::ZeroVector;
    int Pending=-1;
    TArray<FWarlockExpertThreat> Threats;
    TArray<FVector> Pillars;
};
