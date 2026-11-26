#pragma once
#include "Animation/AnimInstance.h"
#include "WarlockAnimation.generated.h"

// Native pose graph: idle/walk blend, upper-body casting, full-body death.
UCLASS(Transient)
class WARLOCKARENA_API UWarlockAnimation : public UAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient, BlueprintReadOnly, Category="Motion") float LocomotionBlend = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Motion") float CastingBlend = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Motion") bool bPlayingDeath = false;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
