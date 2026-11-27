#include "WarlockViewport.h"
#include "WarlockGame.h"
#include "WarlockUserSettings.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Console.h"
#include "GameMapsSettings.h"

int UWarlockViewport::PadPlayer(int PadIndex) const
{
    if(!GetWorld()||!GetWorld()->GetGameInstance())return -1;
    for(int I=0;I<GetWorld()->GetGameInstance()->GetNumLocalPlayers();++I)
        if(auto C=Cast<AWarlockController>(UGameplayStatics::GetPlayerController(GetWorld(),I));C&&C->UsesGamepad()&&C->LocalPadIndex==PadIndex)return I;
    return -1;
}
AWarlockController* UWarlockViewport::DeviceOwner(bool Mouse) const
{
    if(!GetWorld()||!GetWorld()->GetGameInstance())return nullptr;
    for(int I=0;I<GetWorld()->GetGameInstance()->GetNumLocalPlayers();++I)
        if(auto C=Cast<AWarlockController>(UGameplayStatics::GetPlayerController(GetWorld(),I));C&&(Mouse?C->UsesMouse():C->UsesKeyboard()))return C;
    return nullptr;
}
void UWarlockViewport::StartLocalSetup(int32 Count)
{
    auto First=Cast<AWarlockController>(UGameplayStatics::GetPlayerController(GetWorld(),0));
    auto Mode=GetWorld()->GetAuthGameMode<AWarlockMode>();
    if(!First||!Mode||GetWorld()->GetNetMode()!=NM_Standalone||!UGameplayStatics::IsGamePaused(GetWorld())||Count<1||Count>2){if(First)First->SetNotice(TEXT("人数设置用于本机离线对局"));return;}
    int Before=GetWorld()->GetGameInstance()->GetNumLocalPlayers();
    if(Count==2&&Before<2&&!UGameplayStatics::CreatePlayer(First,-1,true)){First->SetNotice(TEXT("无法创建第二位玩家"));return;}
    if(Count==1&&Before>1)UGameplayStatics::RemovePlayer(UGameplayStatics::GetPlayerController(GetWorld(),1),true);
    UGameMapsSettings::GetGameMapsSettings()->TwoPlayerSplitscreenLayout=ETwoPlayerSplitScreenType::Vertical;
    UGameMapsSettings::GetGameMapsSettings()->SetSkipAssigningGamepadToPlayer1(false);LayoutPlayers();
    if(Before!=Count){First->ApplyLocalRoles(Count==1?0:4,3);Mode->EnsureRoster();Mode->RestartMatch(Mode->Match()->bTeams);}
    for(int I=0;I<Count;++I)if(auto C=Cast<AWarlockController>(UGameplayStatics::GetPlayerController(GetWorld(),I)))C->FlushLocalControls();
    First->OpenPausePage(2);UGameplayStatics::SetGamePaused(First,true);
    First->Notice.Empty();
    TargetPlayers=Count;PendingDevices.Init(-2,Count);JoinSeat=0;bAssigningDevices=true;
}
bool UWarlockViewport::ClaimDevice(int32 PadIndex)
{
    if(!bAssigningDevices||!UGameplayStatics::IsGamePaused(GetWorld())||PadIndex<-1||PadIndex>15||!PendingDevices.IsValidIndex(JoinSeat))return false;
    auto First=Cast<AWarlockController>(UGameplayStatics::GetPlayerController(GetWorld(),0));if(!First)return false;
    if(PendingDevices.Contains(PadIndex)){First->SetNotice(TEXT("这台设备已经属于另一名玩家，请用另一台设备确认"));return false;}
    PendingDevices[JoinSeat++]=PadIndex;
    First->Notice.Empty();
    if(JoinSeat==TargetPlayers)
    {
        for(int I=0;I<TargetPlayers;++I)if(auto C=Cast<AWarlockController>(UGameplayStatics::GetPlayerController(GetWorld(),I)))
        {C->LocalControl=PendingDevices[I]<0?4:3;C->LocalPadIndex=FMath::Max(0,PendingDevices[I]);C->FlushLocalControls();}
        bAssigningDevices=false;First->SetNotice(TEXT("设备分配完成，Esc / Menu 继续游戏"));
    }
    return true;
}
void UWarlockViewport::BackLocalSetup()
{
    if(!bAssigningDevices)return;
    if(JoinSeat>0)PendingDevices[--JoinSeat]=-2;
    else {bAssigningDevices=false;PendingDevices.Reset();}
}

bool UWarlockViewport::InputKey(const FInputKeyEventArgs& Args)
{
    if(ViewportConsole&&ViewportConsole->ConsoleActive())return Super::InputKey(Args);
    auto First=Cast<AWarlockController>(UGameplayStatics::GetPlayerController(GetWorld(),0));
    if(First&&First->IsMenuOpen())
    {
        if(bAssigningDevices)
        {
            if(Args.Event==IE_Pressed)
            {
                if(Args.Key==EKeys::Escape||Args.Key==EKeys::BackSpace||Args.Key==EKeys::Gamepad_FaceButton_Right)BackLocalSetup();
                else if(Args.Key==EKeys::Gamepad_FaceButton_Bottom)ClaimDevice(Args.ControllerId);
                else if(Args.Key==EKeys::Enter)ClaimDevice(-1);
            }
            if(Args.Key!=EKeys::LeftMouseButton)return true;
        }
        if(First->WaitingBinding>=0)
        {
            if(Args.Event==IE_Pressed)
            {
                if(Args.Key==EKeys::Escape||Args.Key==EKeys::BackSpace||Args.Key==EKeys::Gamepad_FaceButton_Right)First->WaitingBinding=-1;
                else if(!Args.Key.IsGamepadKey()&&!Args.Key.IsMouseButton())
                {auto S=Cast<UWarlockUserSettings>(UGameUserSettings::GetGameUserSettings());if(S&&S->SetKeyboardBinding(First->WaitingBinding,Args.Key))
                    {First->WaitingBinding=-1;for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)if(auto C=Cast<AWarlockController>(It->Get());C&&C->IsLocalController())C->RefreshKeyboardMappings();}
                    else First->SetNotice(TEXT("该按键保留给系统或菜单，请选择其他键"));}
            }
            return true;
        }
        if(Args.Event==IE_Pressed&&Args.Key==EKeys::F9){First->OpenPausePage(2);return true;}
        if(Args.Event==IE_Pressed&&Args.Key==EKeys::F10){First->OpenPausePage(3);return true;}
        // Settings remain responsive while the world/Enhanced Input simulation is paused.
        if(Args.Event==IE_Pressed&&(Args.Key==EKeys::Tab||(Args.Key==EKeys::Gamepad_FaceButton_Left&&PadPlayer(Args.ControllerId)>=0))){First->ToggleDisplaySettings();return true;}
        if(Args.Key==EKeys::LeftMouseButton)
        {
            if(Args.Event==IE_Pressed)for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)if(auto C=Cast<AWarlockController>(It->Get());C&&C->IsLocalController())
            {float X,Y;int W,H;C->GetViewportSize(W,H);auto L=C->GetLocalPlayer();auto HUD=Cast<AWarlockHUD>(C->GetHUD());
                if(L&&HUD&&C->GetMousePosition(X,Y)){FVector2D Point(X-L->Origin.X*W,Y-L->Origin.Y*H);for(auto& Box:HUD->HitBoxMap)if(Box.Contains(Point)){HUD->NotifyHitBoxClick(Box.GetName());return true;}}}
            return true;
        }
        if(Args.Event==IE_Pressed&&Args.Key==EKeys::Escape&&First->bKeybindMenuOpen){First->OpenPausePage(0);return true;}
        if(Args.Event==IE_Pressed&&(Args.Key==EKeys::Escape||(Args.Key==EKeys::Gamepad_Special_Right&&PadPlayer(Args.ControllerId)>=0))){First->TogglePause();return true;}
        if(Args.Key.IsGamepadKey()&&PadPlayer(Args.ControllerId)<0)return true;
        if(First->bKeybindMenuOpen)
        {
            if(Args.Event==IE_Pressed)
            {
                if(Args.Key==EKeys::Up||Args.Key==EKeys::Gamepad_DPad_Up)First->SelectedBinding=(First->SelectedBinding+21)%22;
                if(Args.Key==EKeys::Down||Args.Key==EKeys::Gamepad_DPad_Down)First->SelectedBinding=(First->SelectedBinding+1)%22;
                if(Args.Key==EKeys::Left||Args.Key==EKeys::Gamepad_DPad_Left||Args.Key==EKeys::Right||Args.Key==EKeys::Gamepad_DPad_Right)First->SelectedBinding=(First->SelectedBinding+11)%22;
                if(Args.Key==EKeys::Enter||Args.Key==EKeys::Gamepad_FaceButton_Bottom)First->StartKeyCapture(First->SelectedBinding);
                if(Args.Key==EKeys::Home||Args.Key==EKeys::Gamepad_LeftShoulder)First->ApplyKeyPreset(0);
                if(Args.Key==EKeys::End||Args.Key==EKeys::Gamepad_RightShoulder)First->ApplyKeyPreset(1);
                if(Args.Key==EKeys::BackSpace||Args.Key==EKeys::Gamepad_FaceButton_Right)First->OpenPausePage(0);
            }
        }
        else if(First->bPlayersMenuOpen)
        {
            if(Args.Event==IE_Pressed)
            {
                if(Args.Key==EKeys::One||Args.Key==EKeys::Left||Args.Key==EKeys::Gamepad_DPad_Left)StartLocalSetup(1);
                if(Args.Key==EKeys::Two||Args.Key==EKeys::Right||Args.Key==EKeys::Gamepad_DPad_Right)StartLocalSetup(2);
                if(Args.Key==EKeys::Enter||Args.Key==EKeys::Gamepad_FaceButton_Bottom)StartLocalSetup(GetWorld()->GetGameInstance()->GetNumLocalPlayers());
                if(Args.Key==EKeys::PageUp||Args.Key==EKeys::Gamepad_RightShoulder){auto M=GetWorld()->GetGameState<AWarlockMatchState>();First->SetParticipantCount(FMath::Min(8,M->ParticipantCount+1));}
                if(Args.Key==EKeys::PageDown||Args.Key==EKeys::Gamepad_LeftShoulder){auto M=GetWorld()->GetGameState<AWarlockMatchState>();int Minimum=FMath::Max(2,GetWorld()->GetGameInstance()->GetNumLocalPlayers()+(M->bTeams?1:0));First->SetParticipantCount(FMath::Max(Minimum,M->ParticipantCount-1));}
                if(Args.Key==EKeys::BackSpace||Args.Key==EKeys::Gamepad_FaceButton_Right)First->OpenPausePage(0);
            }
        }
        else if(First->bDisplaySettingsOpen)
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
        else if(Args.Event==IE_Pressed){const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six};for(int I=0;I<6;++I)if(Args.Key==Keys[I]){First->SetAIDifficulty(I+1);return true;}
            if((Args.Key==EKeys::Gamepad_DPad_Left||Args.Key==EKeys::Gamepad_DPad_Right)&&PadPlayer(Args.ControllerId)>=0){auto M=GetWorld()->GetGameState<AWarlockMatchState>();First->SetAIDifficulty(FMath::Clamp(M->AIDifficulty+(Args.Key==EKeys::Gamepad_DPad_Left?1:-1),1,6));return true;}}
        return true;
    }
    if(First&&(Args.Key==EKeys::Escape||Args.Key==EKeys::Gamepad_Special_Right))
    {if(Args.Key==EKeys::Escape||PadPlayer(Args.ControllerId)>=0)if(Args.Event==IE_Pressed)First->TogglePause();return true;}
    if(Args.Key.IsGamepadKey())
    {int Player=PadPlayer(Args.ControllerId);if(Player>=0)if(auto C=UGameplayStatics::GetPlayerController(GetWorld(),Player))C->InputKey(Args);return true;}
    if(First&&(Args.Key==EKeys::F2||Args.Key==EKeys::F3||Args.Key==EKeys::F4||Args.Key==EKeys::F5||Args.Key==EKeys::F6||Args.Key==EKeys::F7||Args.Key==EKeys::F8)){First->InputKey(Args);return true;}
    if(auto Owner=DeviceOwner(Args.Key.IsMouseButton());Owner&&Owner!=First){Owner->InputKey(Args);return true;}
    // Preserve the engine console/fullscreen and ordinary P1 UI input path.
    // P1's role filter still rejects gameplay input belonging to nobody.
    return Super::InputKey(Args);
}
bool UWarlockViewport::InputAxis(const FInputKeyEventArgs& Args)
{
    if(UGameplayStatics::IsGamePaused(GetWorld()))return true;
    if(Args.Key.IsGamepadKey()){int Player=PadPlayer(Args.ControllerId);if(Player>=0)if(auto C=UGameplayStatics::GetPlayerController(GetWorld(),Player))C->InputKey(Args);return true;}
    bool Mouse=Args.Key==EKeys::MouseX||Args.Key==EKeys::MouseY||Args.Key==EKeys::MouseWheelAxis;
    if(auto Owner=DeviceOwner(Mouse))Owner->InputKey(Args);return true;
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
