#include "WarlockViewport.h"
#include "WarlockGame.h"
#include "WarlockUserSettings.h"
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
    if(First&&UGameplayStatics::IsGamePaused(GetWorld()))
    {
        // Settings remain responsive while the world/Enhanced Input simulation is paused.
        if(Args.Event==IE_Pressed&&(Args.Key==EKeys::Tab||(Args.Key==EKeys::Gamepad_FaceButton_Left&&PadPlayer(Args.ControllerId)>=0))){First->ToggleDisplaySettings();return true;}
        if(Args.Key==EKeys::LeftMouseButton)
        {
            if(Args.Event==IE_Pressed)for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)if(auto C=Cast<AWarlockController>(It->Get());C&&C->IsLocalController()&&C->UsesMouse())
            {float X,Y;int W,H;C->GetViewportSize(W,H);auto L=C->GetLocalPlayer();auto HUD=Cast<AWarlockHUD>(C->GetHUD());
                if(L&&HUD&&C->GetMousePosition(X,Y)){FVector2D Point(X-L->Origin.X*W,Y-L->Origin.Y*H);for(auto& Box:HUD->HitBoxMap)if(Box.Contains(Point)){HUD->NotifyHitBoxClick(Box.GetName());return true;}}}
            return true;
        }
        if(First->bDisplaySettingsOpen)
        {
            bool Pad=Args.Key.IsGamepadKey();if(Pad&&PadPlayer(Args.ControllerId)<0)return true;
            if(Args.Event==IE_Pressed)
            {
                if(Args.Key==EKeys::Up||Args.Key==EKeys::Gamepad_DPad_Up)First->SelectedDisplayRow=(First->SelectedDisplayRow+UWarlockUserSettings::RowCount-1)%UWarlockUserSettings::RowCount;
                if(Args.Key==EKeys::Down||Args.Key==EKeys::Gamepad_DPad_Down)First->SelectedDisplayRow=(First->SelectedDisplayRow+1)%UWarlockUserSettings::RowCount;
                if(Args.Key==EKeys::Left||Args.Key==EKeys::Gamepad_DPad_Left)First->EditDisplaySetting(First->SelectedDisplayRow,-1);
                if(Args.Key==EKeys::Right||Args.Key==EKeys::Gamepad_DPad_Right||Args.Key==EKeys::Enter||Args.Key==EKeys::Gamepad_FaceButton_Bottom)First->EditDisplaySetting(First->SelectedDisplayRow,1);
            }
        }
        else if(Args.Event==IE_Pressed){const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five};for(int I=0;I<5;++I)if(Args.Key==Keys[I]){First->SetAIDifficulty(I+1);return true;}
            if((Args.Key==EKeys::Gamepad_DPad_Left||Args.Key==EKeys::Gamepad_DPad_Right)&&PadPlayer(Args.ControllerId)>=0){auto M=GetWorld()->GetGameState<AWarlockMatchState>();First->SetAIDifficulty(FMath::Clamp(M->AIDifficulty+(Args.Key==EKeys::Gamepad_DPad_Left?1:-1),1,5));return true;}}
    }
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
