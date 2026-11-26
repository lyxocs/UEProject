#pragma once
#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "WarlockViewport.generated.h"

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
};
