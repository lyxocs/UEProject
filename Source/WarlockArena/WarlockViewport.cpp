#include "WarlockViewport.h"
#include "WarlockGame.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"
#include "Engine/GameInstance.h"

int UWarlockViewport::PadPlayer(int PadIndex) const
{
    auto First=Cast<AWarlockController>(UGameplayStatics::GetPlayerController(GetWorld(),0));if(!First)return -1;
    if(GetWorld()->GetGameInstance()->GetNumLocalPlayers()<2)return First->UsesGamepad()&&PadIndex==0?0:-1;
    if(First->LocalControl==4)return PadIndex==0?1:-1;
    if(First->LocalControl==3)return PadIndex>=0&&PadIndex<2?PadIndex:-1;
    return -1;
}

bool UWarlockViewport::InputKey(const FInputKeyEventArgs& Args)
{
    auto First=Cast<AWarlockController>(UGameplayStatics::GetPlayerController(GetWorld(),0));
    if(First&&(Args.Key==EKeys::Escape||Args.Key==EKeys::Gamepad_Special_Right))
    {if(Args.Key==EKeys::Escape||PadPlayer(Args.ControllerId)>=0)if(Args.Event==IE_Pressed)First->TogglePause();return true;}
    if(Args.Key.IsGamepadKey())
    {int Player=PadPlayer(Args.ControllerId);if(Player>=0)if(auto C=UGameplayStatics::GetPlayerController(GetWorld(),Player))C->InputKey(Args);return true;}
    if(First&&First->LocalControl==1&&Args.Key.IsMouseButton())
    {if(auto Second=UGameplayStatics::GetPlayerController(GetWorld(),1))Second->InputKey(Args);return true;}
    return Super::InputKey(Args);
}
bool UWarlockViewport::InputAxis(const FInputKeyEventArgs& Args)
{
    if(Args.Key.IsGamepadKey()){int Player=PadPlayer(Args.ControllerId);if(Player>=0)if(auto C=UGameplayStatics::GetPlayerController(GetWorld(),Player))C->InputKey(Args);return true;}
    auto First=Cast<AWarlockController>(UGameplayStatics::GetPlayerController(GetWorld(),0));
    if(First&&First->LocalControl==1&&(Args.Key==EKeys::MouseX||Args.Key==EKeys::MouseY||Args.Key==EKeys::MouseWheelAxis))
    {if(auto Second=UGameplayStatics::GetPlayerController(GetWorld(),1))Second->InputKey(Args);return true;}
    return Super::InputAxis(Args);
}
bool UWarlockViewport::InjectPadKey(FName Key,int PadIndex,bool Pressed)
{
#if !UE_BUILD_SHIPPING
    FInputKeyEventArgs Args(Viewport,FInputDeviceId::CreateFromInternalId(0),FKey(Key),Pressed?IE_Pressed:IE_Released,FPlatformTime::Cycles64());Args.ControllerId=PadIndex;return InputKey(Args);
#else
    return false;
#endif
}
bool UWarlockViewport::InjectPadAxis(FName Key,int PadIndex,float Value)
{
#if !UE_BUILD_SHIPPING
    FInputKeyEventArgs Args(Viewport,FInputDeviceId::CreateFromInternalId(0),FKey(Key),Value,1.f/60,1,FPlatformTime::Cycles64());Args.ControllerId=PadIndex;return InputAxis(Args);
#else
    return false;
#endif
}
bool UWarlockViewport::InjectLocalKey(FName Key,bool Pressed)
{
#if !UE_BUILD_SHIPPING
    return InputKey(FInputKeyEventArgs(Viewport,FInputDeviceId::CreateFromInternalId(0),FKey(Key),Pressed?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));
#else
    return false;
#endif
}
