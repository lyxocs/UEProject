#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WarlockAudio.generated.h"

class USoundWave;
class UAudioComponent;
class USoundConcurrency;

// One mix per world, including local split-screen. No audio work in training/DS.
UCLASS()
class WARLOCKARENA_API UWarlockAudio : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<USoundWave>> CastSounds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<USoundWave>> HitSounds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UAudioComponent> Music;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UAudioComponent> BattleLayer;
    UPROPERTY() TObjectPtr<USoundConcurrency> Concurrency;
    UPROPERTY() TObjectPtr<USoundWave> Stone;
    UPROPERTY() TObjectPtr<USoundWave> Ground;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int PlayedEvents=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int LastSlot=-1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int LastSurface=-1;
    TMap<int,double> LastPlayed;
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
    virtual void OnWorldBeginPlay(UWorld& World) override;
    virtual void Deinitialize() override;
    virtual void Tick(float Dt) override;
    virtual bool IsTickableWhenPaused() const override{return true;}
    virtual TStatId GetStatId() const override {RETURN_QUICK_DECLARE_CYCLE_STAT(UWarlockAudio,STATGROUP_Tickables);}
    // Surface 0 release, 1 character, 2 stone, 3 ground. Never applies damage.
    UFUNCTION(BlueprintCallable) void PlayEvent(int Slot,int Surface,FVector Position);
    static void Event(UWorld* World,int Slot,int Surface,FVector Position);
    UFUNCTION(BlueprintCallable) void PlayCelebration(int Kind,int Streak=0);
};
