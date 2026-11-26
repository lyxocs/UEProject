#pragma once
#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "WarlockViewport.generated.h"
class AWarlockController;

UCLASS()
class WARLOCKARENA_API UWarlockViewport : public UGameViewportClient
{
    GENERATED_BODY()
public:
    virtual bool InputKey(const FInputKeyEventArgs& Args) override;
    virtual bool InputAxis(const FInputKeyEventArgs& Args) override;
    UFUNCTION(BlueprintCallable, Category="Validation") bool InjectLocalKey(FName Key, bool Pressed);
    UFUNCTION(BlueprintCallable, Category="Validation") bool InjectPadKey(FName Key, int PadIndex, bool Pressed);
    UFUNCTION(BlueprintCallable, Category="Validation") bool InjectPadAxis(FName Key, int PadIndex, float Value);
    int PadPlayer(int PadIndex) const;
    UPROPERTY(BlueprintReadOnly) bool bAssigningDevices=false;
    UPROPERTY(BlueprintReadOnly) int32 JoinSeat=0;
    UPROPERTY(BlueprintReadOnly) int32 TargetPlayers=1;
    UPROPERTY(BlueprintReadOnly) TArray<int32> PendingDevices;
    UFUNCTION(BlueprintCallable) void StartLocalSetup(int32 Count);
    UFUNCTION(BlueprintCallable) bool ClaimDevice(int32 PadIndex);
    UFUNCTION(BlueprintCallable) void BackLocalSetup();
    AWarlockController* DeviceOwner(bool Mouse) const;
};
