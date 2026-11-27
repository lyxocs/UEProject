#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "WarlockUserSettings.generated.h"

// One shared local display configuration for both split-screen players.
UCLASS(Config=GameUserSettings)
class WARLOCKARENA_API UWarlockUserSettings : public UGameUserSettings
{
    GENERATED_BODY()
public:
    UWarlockUserSettings();
    UPROPERTY(Config, BlueprintReadOnly) TArray<FKey> KeyboardBindings;
    UFUNCTION(BlueprintPure) FKey KeyboardKey(int32 Index) const;
    UFUNCTION(BlueprintCallable) bool SetKeyboardBinding(int32 Index,FKey Key);
    UFUNCTION(BlueprintCallable) void ResetKeyboardBindings();
    static const TCHAR* BindingName(int32 Index);
    static const TCHAR* ActionName(int32 Index);
    static constexpr int32 BindingCount=22;
    UPROPERTY(Config, BlueprintReadOnly) bool bClientSettingsInitialized=false;
    UPROPERTY(Config, BlueprintReadOnly) int32 GraphicsPreset=2;
    // 0: high-quality TAAU, 1: balanced TSR, 2: high-quality TSR history.
    UPROPERTY(Config, BlueprintReadOnly) int32 AntiAliasingMode=1;
    UPROPERTY(Config, BlueprintReadOnly) float MusicVolume=.28f;
    UPROPERTY(Config, BlueprintReadOnly) float EffectsVolume=.75f;
    UPROPERTY(Config, BlueprintReadOnly) int CastTriggerMode=1;
    virtual void ApplyNonResolutionSettings() override;
    void InitializeClient();
    UFUNCTION(BlueprintCallable) void SelectPreset(int32 Preset);
    UFUNCTION(BlueprintCallable) void Adjust(int32 Row,int32 Direction);
    UFUNCTION(BlueprintPure) FString Label(int32 Row) const;
    static const TCHAR* RowName(int32 Row);
    static const TCHAR* PresetName(int32 Preset);
    static constexpr int32 RowCount=14;
};
