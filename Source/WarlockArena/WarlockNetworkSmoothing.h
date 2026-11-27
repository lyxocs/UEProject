#pragma once
#include "CoreMinimal.h"

// Arrival-time interpolation, entirely cosmetic. No prediction of hits or rules.
// Bound history and delay; stop at the latest snapshot during a network stall.
struct FWarlockNetworkSmoothing
{
    struct FSample {double Time;FVector Position;FQuat Rotation;};
    TArray<FSample,TInlineAllocator<12>> Samples;
    static constexpr double Delay=.075;
    void Reset(double Now,FVector Position,FQuat Rotation)
    {Samples.Reset();Samples.Add({Now-Delay,Position,Rotation});Samples.Add({Now,Position,Rotation});}
    void Push(double Now,FVector Position,FQuat Rotation,bool Snap=false)
    {
        if(Snap||Samples.IsEmpty()){Reset(Now,Position,Rotation);return;}
        if(Now-Samples.Last().Time<.001){Samples.Last()={Now,Position,Rotation};return;}
        Samples.Add({Now,Position,Rotation});
        if(Samples.Num()>12)Samples.RemoveAt(0);
    }
    FTransform Evaluate(double Now) const
    {
        if(Samples.IsEmpty())return FTransform::Identity;
        const double Target=Now-Delay;
        for(int I=1;I<Samples.Num();++I)if(Target<Samples[I].Time)
        {
            const auto& A=Samples[I-1];const auto& B=Samples[I];
            const float T=FMath::Clamp(float((Target-A.Time)/FMath::Max(.0001,B.Time-A.Time)),0.f,1.f);
            return FTransform(FQuat::Slerp(A.Rotation,B.Rotation,T),FMath::Lerp(A.Position,B.Position,T));
        }
        return FTransform(Samples.Last().Rotation,Samples.Last().Position);
    }
};

bool WarlockNetworkSmoothingEnabled();
