#include "WarlockGame.h"
#include "WarlockCatalog.h"
#include "WarlockTuning.h"
#include "WarlockUserSettings.h"
#include "WarlockViewport.h"
#include "WarlockOnline.h"
#include "InputKeyEventArgs.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/SpringArmComponent.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/GameUserSettings.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
FKey BindingKey(int Index)
{auto S=Cast<UWarlockUserSettings>(UGameUserSettings::GetGameUserSettings());return (S?S:GetDefault<UWarlockUserSettings>())->KeyboardKey(Index);}
}

AWarlockController::AWarlockController(){bShowMouseCursor=true;bEnableClickEvents=true;DefaultMouseCursor=EMouseCursor::Crosshairs;}
AWarlockPawn* AWarlockController::Fighter() const{return Cast<AWarlockPawn>(GetPawn());}
bool AWarlockController::InputKey(const FInputKeyEventArgs& Args)
{
    if(Args.Key.IsGamepadKey()&&!UsesGamepad())return true;
    bool Mouse=Args.Key.IsMouseButton()||Args.Key==EKeys::MouseX||Args.Key==EKeys::MouseY||Args.Key==EKeys::MouseWheelAxis;
    if(Mouse&&!UsesMouse())return true;
    if(auto Room=GetWorld()->GetGameState<AWarlockMatchState>();Room&&Room->Phase==TEXT("LOBBY")&&!IsMenuOpen()&&Args.Event==IE_Pressed)
    {
        if(Args.Key==EKeys::Enter||Args.Key==BindingKey(14)||Args.Key==EKeys::Gamepad_FaceButton_Bottom){ToggleReady();return true;}
        if(Args.Key==EKeys::Gamepad_DPad_Left||Args.Key==EKeys::Gamepad_DPad_Right){ServerRoomOptions(Room->ParticipantCount+(Args.Key==EKeys::Gamepad_DPad_Left?-1:1),Room->AIDifficulty,Room->bTeams);return true;}
        if(Args.Key==EKeys::Gamepad_RightShoulder){ServerRoomOptions(Room->ParticipantCount,Room->AIDifficulty%6+1,Room->bTeams);return true;}
        if(Args.Key==EKeys::Gamepad_FaceButton_Top){ServerRoomOptions(Room->ParticipantCount,Room->AIDifficulty,!Room->bTeams);return true;}
    }
    if(!UsesKeyboard()&&!Args.Key.IsGamepadKey()&&!Mouse&&Args.Key!=EKeys::F2&&Args.Key!=EKeys::F3&&Args.Key!=EKeys::F8&&Args.Key!=EKeys::F5&&Args.Key!=EKeys::F4&&Args.Key!=EKeys::F6&&Args.Key!=EKeys::F7)return true;
    // Secondary local mouse controllers do not always receive Enhanced Input's
    // Completed callback after the viewport processes a mouse-up. Consume the
    // owned raw release too; MouseHeldSlot makes duplicate completion harmless.
    if(UsesMouse()&&Args.Key==EKeys::LeftMouseButton&&Args.Event==IE_Released)MousePrimaryReleased();
    if(!bShopOpen&&!IsMenuOpen()&&Args.Event==IE_Pressed&&(Args.Key==BindingKey(13)||Args.Key==EKeys::MiddleMouseButton||Args.Key==EKeys::Gamepad_RightThumbstick))
    {StopAction();return true;}
    if(((Args.Key==EKeys::Gamepad_FaceButton_Top&&bShopOpen)||(Args.Key==EKeys::Gamepad_DPad_Up&&!bShopOpen))&&!bHelpOpen)if(auto M=GetWorld()->GetGameState<AWarlockMatchState>();M&&M->Phase==TEXT("SHOP")){if(Args.Event==IE_Pressed)ToggleReady();return true;}
    return Super::InputKey(Args);
}
UEnhancedInputLocalPlayerSubsystem* AWarlockController::LocalInput() const{return GetLocalPlayer()?ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()):nullptr;}
void AWarlockController::BeginPlay()
{
    Super::BeginPlay();if(!IsLocalController())return;
    if(auto GI=GetGameInstance<UWarlockOnline>())
    {
        if(GetNetMode()==NM_Client){ServerDisplayName(GI->DisplayName);GI->Status.Empty();}
        else if(GI->Status.StartsWith(TEXT("连接已断开"))||GI->Status.StartsWith(TEXT("无法进入")))GetWorldTimerManager().SetTimerForNextTick([GI](){GI->ShowConnectionPanel();});
    }
    SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
    if(!GIsEditor&&GetGameInstance()->GetFirstLocalPlayerController()==this)
    {
        if(auto Settings=Cast<UWarlockUserSettings>(UGameUserSettings::GetGameUserSettings()))Settings->InitializeClient();
    }
}
void AWarlockController::SetupInputComponent()
{
    Super::SetupInputComponent();auto E=Cast<UEnhancedInputComponent>(InputComponent);if(!E||!GetLocalPlayer())return;
    Mapping=NewObject<UInputMappingContext>(this);
    auto Action=[&](const TCHAR* Name,EInputActionValueType Type)
    {auto A=NewObject<UInputAction>(this,FName(Name));A->ValueType=Type;Actions.Add(A);return A;};
    auto MoveAction=Action(TEXT("Move"),EInputActionValueType::Axis2D);
    Mapping->MapKey(MoveAction,EKeys::Gamepad_Left2D);
    auto AimAction=Action(TEXT("Aim"),EInputActionValueType::Axis2D);
    // Facing is selected with movement, never with the right stick.
    for(auto A:{MoveAction,AimAction}){auto Dead=NewObject<UInputModifierDeadZone>(this);Dead->LowerThreshold=.18f;Dead->Type=EDeadZoneType::Radial;A->Modifiers.Add(Dead);}
    E->BindAction(MoveAction,ETriggerEvent::Triggered,this,&AWarlockController::Move);E->BindAction(MoveAction,ETriggerEvent::Completed,this,&AWarlockController::Move);
    E->BindAction(AimAction,ETriggerEvent::Triggered,this,&AWarlockController::Aim);E->BindAction(AimAction,ETriggerEvent::Completed,this,&AWarlockController::Aim);
    const FKey Pad[]={EKeys::Gamepad_RightTrigger,EKeys::Gamepad_LeftTrigger,EKeys::Gamepad_FaceButton_Left,EKeys::Gamepad_FaceButton_Bottom,EKeys::Gamepad_FaceButton_Top,EKeys::Gamepad_FaceButton_Right,EKeys::Gamepad_LeftShoulder,EKeys::Gamepad_RightShoulder};
    for(int I=0;I<8;++I)
    {auto A=Action(*FString::Printf(TEXT("Cast%d"),I),EInputActionValueType::Boolean);Mapping->MapKey(A,BindingKey(I+4));Mapping->MapKey(A,Pad[I]);E->BindAction(A,ETriggerEvent::Started,this,&AWarlockController::CastAction,I);E->BindAction(A,ETriggerEvent::Completed,this,&AWarlockController::ReleaseTargeting,I);}
    auto Bind=[&](const TCHAR* Name,FKey Key,auto Method)
    {auto A=Action(Name,EInputActionValueType::Boolean);Mapping->MapKey(A,Key);E->BindAction(A,ETriggerEvent::Started,this,Method);return A;};
    auto Shop=Bind(TEXT("Shop"),BindingKey(12),&AWarlockController::ToggleShop);Mapping->MapKey(Shop,EKeys::Gamepad_Special_Left);
    Bind(TEXT("ClickMove"),EKeys::RightMouseButton,&AWarlockController::ClickMove);
    auto Primary=Bind(TEXT("MousePrimary"),EKeys::LeftMouseButton,&AWarlockController::MousePrimary);E->BindAction(Primary,ETriggerEvent::Completed,this,&AWarlockController::MousePrimaryReleased);
    Bind(TEXT("MouseSmite"),EKeys::ThumbMouseButton2,&AWarlockController::MouseSecondary);
    Bind(TEXT("MouseShop"),EKeys::ThumbMouseButton,&AWarlockController::ToggleShop);
    auto Stop=Bind(TEXT("Stop"),BindingKey(13),&AWarlockController::StopAction);Mapping->MapKey(Stop,EKeys::MiddleMouseButton);Mapping->MapKey(Stop,EKeys::Gamepad_RightThumbstick);
    Bind(TEXT("TwoPads"),EKeys::F2,&AWarlockController::AddLocalPlayer);
    Bind(TEXT("KeyboardAndPad"),EKeys::F3,&AWarlockController::AddMixedPlayer);
    Bind(TEXT("KeyboardMouseSplit"),EKeys::F8,&AWarlockController::AddKeyboardMousePlayer);
    Bind(TEXT("Teams"),EKeys::F4,&AWarlockController::ToggleTeams);
    Bind(TEXT("Layout"),EKeys::F5,&AWarlockController::ToggleSplitLayout);
    Bind(TEXT("Help"),BindingKey(21),&AWarlockController::ToggleHelp);
    auto Outfit=Bind(TEXT("Appearance"),BindingKey(20),&AWarlockController::CycleAppearance);Mapping->MapKey(Outfit,EKeys::Gamepad_LeftThumbstick);
    Bind(TEXT("Resolution"),EKeys::F6,&AWarlockController::CycleResolution);
    Bind(TEXT("Quality"),EKeys::F7,&AWarlockController::CycleQuality);
    auto ZoomAction=Action(TEXT("Zoom"),EInputActionValueType::Axis1D);Mapping->MapKey(ZoomAction,EKeys::MouseWheelAxis);E->BindAction(ZoomAction,ETriggerEvent::Triggered,this,&AWarlockController::MouseWheel);
    Bind(TEXT("Mode"),BindingKey(16),&AWarlockController::ToggleMode);
    Bind(TEXT("Sell"),BindingKey(18),&AWarlockController::SellSelected);
    Bind(TEXT("Alternative"),BindingKey(17),&AWarlockController::AlternativeSelected);
    Bind(TEXT("Stone"),BindingKey(19),&AWarlockController::StoneSelected);
    Bind(TEXT("Confirm"),BindingKey(15),&AWarlockController::ConfirmShop);
    Bind(TEXT("Ready"),BindingKey(14),&AWarlockController::ToggleReady);
    const FKey NavPads[]={EKeys::Gamepad_DPad_Up,EKeys::Gamepad_DPad_Down,EKeys::Gamepad_DPad_Left,EKeys::Gamepad_DPad_Right};const int NavSteps[]={-4,4,-1,1};
    for(int I=0;I<4;++I){auto A=Action(UWarlockUserSettings::ActionName(I),EInputActionValueType::Boolean);Mapping->MapKey(A,BindingKey(I));Mapping->MapKey(A,NavPads[I]);E->BindAction(A,ETriggerEvent::Started,this,&AWarlockController::NavigateShop,NavSteps[I]);}
    for(auto Pair:{TPair<FKey,int>(EKeys::PageUp,-1),TPair<FKey,int>(EKeys::PageDown,1)}){auto A=Action(*FString::Printf(TEXT("ShopPage%d"),Actions.Num()),EInputActionValueType::Boolean);Mapping->MapKey(A,Pair.Key);E->BindAction(A,ETriggerEvent::Started,this,&AWarlockController::NavigateShop,Pair.Value);}
    if(auto Sub=ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))Sub->AddMappingContext(Mapping,0);
}
void AWarlockController::Move(const FInputActionValue& Value)
{
    if(!UsesGamepad()){LocalMove=FVector::ZeroVector;return;}
    auto V=Value.Get<FVector2D>();LocalMove=FVector(V.X,-V.Y,0);
    if(!V.IsNearlyZero()){bGamepadAiming=true;LastAimDirection=LocalMove.GetSafeNormal2D();AimDistance=1200;}
}
void AWarlockController::Aim(const FInputActionValue& Value)
{
    auto V=Value.Get<FVector2D>();if(V.Size()>.05f){LastAimDirection=FVector(V.X,-V.Y,0).GetSafeNormal();AimDistance=250+FMath::Min(V.Size(),1.)*950;bGamepadAiming=true;}
}
void AWarlockController::PlayerTick(float Dt)
{
    Super::PlayerTick(Dt);if(!IsLocalController())return;TickNetworkTest();NoticeTime=FMath::Max(0.f,NoticeTime-Dt);
    if(this==GetWorld()->GetFirstPlayerController())if(auto Settings=Cast<UWarlockUserSettings>(UGameUserSettings::GetGameUserSettings()))Settings->UpdateRenderBudget(Dt);
    auto P=Fighter();if(!P)return;
    for(TActorIterator<AWarlockPawn> It(GetWorld());It;++It)
    {if(It->StealthRemaining>0&&P->IsEnemy(*It)&&FVector::Dist2D(P->GetActorLocation(),It->GetActorLocation())>100)HiddenActors.AddUnique(*It);else HiddenActors.Remove(*It);}
    if(UsesMouse()&&(FMath::Abs(GetInputAnalogKeyState(EKeys::MouseX))>0||FMath::Abs(GetInputAnalogKeyState(EKeys::MouseY))>0))bGamepadAiming=false;
    if(bGamepadAiming||!UsesMouse())LocalAim=P->GetActorLocation()+LastAimDirection*AimDistance;
    else
    {
        FVector Origin,Direction;
        if(DeprojectMousePositionToWorld(Origin,Direction)&&FMath::Abs(Direction.Z)>.001f)
        {LocalAim=Origin+Direction*((40-Origin.Z)/Direction.Z);LastAimDirection=(LocalAim-P->GetActorLocation()).GetSafeNormal2D();}
    }
    FVector MoveDirection=UsesGamepad()?LocalMove:FVector::ZeroVector;
    if(UsesKeyboard()){
    if(IsInputKeyDown(BindingKey(0)))MoveDirection.Y-=1;if(IsInputKeyDown(BindingKey(1)))MoveDirection.Y+=1;
    if(IsInputKeyDown(BindingKey(2)))MoveDirection.X-=1;if(IsInputKeyDown(BindingKey(3)))MoveDirection.X+=1;}
    if(LocalControl==1&&!MoveDirection.IsNearlyZero()){LastAimDirection=MoveDirection.GetSafeNormal2D();AimDistance=1200;LocalAim=P->GetActorLocation()+LastAimDirection*AimDistance;}
    if(bShopOpen||bHelpOpen||IsMenuOpen())MoveDirection=FVector::ZeroVector;
    InputSendTime-=Dt;if(InputSendTime<=0){InputSendTime=.03f;ServerInput(MoveDirection.GetClampedToMaxSize(1),LocalAim);}
    if(auto M=GetWorld()->GetGameState<AWarlockMatchState>();M&&M->Phase!=TEXT("SHOP"))bShopOpen=false;
    UpdateTargetGuide(Dt);
}
void AWarlockController::CastAction(const FInputActionValue& Value,int Slot)
{
    if(IsMenuOpen())return;
    if(auto M=GetWorld()->GetGameState<AWarlockMatchState>();M&&M->Phase==TEXT("FINAL")){if(Slot==3)ConfirmShop();return;}
    if(bHelpOpen){if(Slot==5)bHelpOpen=false;return;}
    if(bShopOpen){if(Slot==3)ConfirmShop();else if(Slot==5)bShopOpen=false;else if(Slot==2)ToggleMode();else if(Slot==0)SellSelected();else if(Slot==1)StoneSelected();else if(Slot==6||Slot==7)SelectedProduct=SelectedProduct<12||SelectedProduct==20?12:0;return;}
    if(bGamepadAiming||LocalControl==1||LocalControl==3)if(auto P=Fighter())LocalAim=P->GetActorLocation()+LastAimDirection*1200;
    StartTargeting(Slot);
}
void AWarlockController::MousePrimary()
{
    if(!UsesMouse()||bShopOpen||bHelpOpen||IsMenuOpen())return;
    if(MouseHeldSlot!=-1)return;
    float X,Y;if(GetMousePosition(X,Y)&&GetHUD()){
        auto L=GetLocalPlayer();int W,H;GetViewportSize(W,H);FVector2D Point(X-L->Origin.X*W,Y-L->Origin.Y*H);
        // HitBoxMap is local to this player's canvas. Avoid the engine helper's
        // additional viewport offset so a P2 skill selection cannot also cast.
        for(const FHUDHitBox& Box:GetHUD()->HitBoxMap)if(Box.ConsumesInput()&&Box.Contains(Point))return;}
    if(AimingSlot>=0){ReleaseTargeting(AimingSlot);MouseHeldSlot=-2;return;}
    MouseHeldSlot=LocalControl==2?SelectedSpell:0;CastAction(FInputActionValue(true),MouseHeldSlot);
}
void AWarlockController::MouseSecondary(){if(UsesMouse())CastAction(FInputActionValue(true),1);}
void AWarlockController::MouseWheel(const FInputActionValue& Value)
{
    float V=Value.Get<float>();if(FMath::IsNearlyZero(V))return;
    if(bShopOpen){ScrollShop(V>0?-1:1);return;}
    if(LocalControl!=2){Zoom(Value);return;}
    int Direction=V>0?-1:1;for(int I=0;I<8;++I){SelectedSpell=(SelectedSpell+Direction+8)%8;if(auto P=Fighter();P&&P->State()&&P->State()->Level(SelectedSpell)>0)break;}
}
void AWarlockController::StopAction(){CancelTargeting();if(bShopOpen){AlternativeSelected();return;}if(!IsMenuOpen())ServerStop();}
void AWarlockController::ClickMove(){CancelTargeting();if(!bShopOpen&&!bHelpOpen&&!IsMenuOpen())ServerClick(LocalAim);}
bool AWarlockController::IsMenuOpen() const{return bOnlineMenuOpen||bConnectionPanelOpen||UGameplayStatics::IsGamePaused(this);}
void AWarlockController::TogglePause(){OpenPausePage(0);if(GetNetMode()==NM_Client||GetNetMode()==NM_ListenServer){bOnlineMenuOpen=!bOnlineMenuOpen;CancelTargeting();ServerStop();return;}for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)if(auto C=Cast<AWarlockController>(It->Get());C&&C->IsLocalController())C->CancelTargeting();ServerToggleMatchPause();}
void AWarlockController::ToggleReady(){if(!bHelpOpen&&!IsMenuOpen())ServerReady();}
void AWarlockController::SetAIDifficulty(int Tier)
{auto First=Cast<AWarlockController>(GetWorld()->GetFirstPlayerController());if(First&&First!=this&&First->IsLocalController())First->ServerAIDifficulty(Tier);else ServerAIDifficulty(Tier);}
void AWarlockController::ServerAIDifficulty_Implementation(int Tier)
{
    auto Mode=GetWorld()->GetAuthGameMode<AWarlockMode>();
    if(!Mode||Tier<1||Tier>6)return;
    if(Mode->Match()->bOnlineRoom){if(Mode->Match()->Phase!=TEXT("LOBBY")||GetPlayerState<AWarlockState>()->GetPlayerId()!=Mode->Match()->RoomHostId)return;}else if(!UGameplayStatics::IsGamePaused(this)||GetWorld()->GetFirstPlayerController()!=this)return;
    Mode->Match()->AIDifficulty=Tier;Mode->Match()->ForceNetUpdate();
    for(auto P:Mode->Fighters)if(auto AI=Cast<AWarlockAI>(P->GetController())){P->CancelCast();AI->ResetPerception();}
    GConfig->SetInt(TEXT("WarlockAI"),TEXT("Difficulty"),Tier,GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);
}
void AWarlockController::ServerReady_Implementation()
{
    auto Mode=GetWorld()->GetAuthGameMode<AWarlockMode>();auto S=GetPlayerState<AWarlockState>();
    if(!Mode||!S||S->bBot||(Mode->Match()->Phase!=TEXT("SHOP")&&Mode->Match()->Phase!=TEXT("LOBBY"))||UGameplayStatics::IsGamePaused(this))return;
    S->bReady=!S->bReady;S->ForceNetUpdate();Mode->CheckReady();
}
FString AWarlockController::ControlLabel() const
{return LocalControl==1?TEXT("键盘"):LocalControl==2?TEXT("鼠标"):LocalControl==3?TEXT("手柄"):LocalControl==4?TEXT("键鼠"):TEXT("单人");}
void AWarlockController::ServerToggleMatchPause_Implementation()
{
    if(GetNetMode()!=NM_Standalone)return;
    bool Paused=UGameplayStatics::IsGamePaused(this);
    if(UGameplayStatics::SetGamePaused(this,!Paused))for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)if(auto C=Cast<AWarlockController>(It->Get())){
        C->LocalMove=FVector::ZeroVector;if(C->PlayerInput)C->PlayerInput->FlushPressedKeys();if(auto P=C->Fighter()){P->MoveInput=FVector::ZeroVector;P->bClickMoving=false;}}
}
void AWarlockController::ServerStop_Implementation(){if(auto P=Fighter()){P->CancelCast();P->MoveInput=FVector::ZeroVector;P->bClickMoving=false;}LocalMove=FVector::ZeroVector;}
void AWarlockController::ToggleShop(){auto M=GetWorld()->GetGameState<AWarlockMatchState>();if(M&&M->Phase==TEXT("SHOP")){bShopOpen=!bShopOpen;CancelTargeting();if(bShopOpen){LocalMove=FVector::ZeroVector;ServerStop();}}else SetNotice(TEXT("商店只在准备阶段开放"));}
void AWarlockController::PreviewCharacter(int Appearance)
{
    if(GetNetMode()!=NM_Standalone)return;
    auto Match=GetWorld()->GetGameState<AWarlockMatchState>();auto P=Fighter();auto S=GetPlayerState<AWarlockState>();
    if(!Match||Match->Phase!=TEXT("SHOP")||!P||!S)return;
    S->Appearance=FMath::Clamp(Appearance,0,2);P->CameraDistance=300;P->CameraArm->TargetArmLength=300;
    P->SetActorRotation(FRotator::ZeroRotator);P->CameraArm->SetRelativeRotation(FRotator(-12,180,0));P->CameraArm->TargetOffset=FVector(0,0,30);
    bShopOpen=false;
}
void AWarlockController::CaptureScene(const FString& Filename,float Delay)
{
    if(!IsLocalController()||Filename.IsEmpty())return;
    FTimerHandle Handle;
    GetWorldTimerManager().SetTimer(Handle,FTimerDelegate::CreateLambda([Filename](){FScreenshotRequest::RequestScreenshot(Filename,true,false);}),FMath::Clamp(Delay,1.f,15.f),false);
}
void AWarlockController::BeginTestRound()
{
#if !UE_BUILD_SHIPPING
    if(GetNetMode()!=NM_Standalone)return;
    if(auto Mode=GetWorld()->GetAuthGameMode<AWarlockMode>();Mode&&Mode->Match()->Phase==TEXT("SHOP"))Mode->StartRound();
#endif
}
void AWarlockController::PreviewSpell(int Slot)
{
#if !UE_BUILD_SHIPPING
    if(GetNetMode()!=NM_Standalone||Slot<0||Slot>7)return;
    auto Mode=GetWorld()->GetAuthGameMode<AWarlockMode>();if(!Mode||!Fighter())return;
    Mode->RestartMatch(false);Mode->StartRound();Mode->bVisualPreview=true;
    auto P=Fighter();P->ResetForRound(FVector::ZeroVector);P->Protection=0;P->State()->Levels.Init(1,8);
    P->CameraDistance=1050;P->CameraArm->TargetArmLength=1050;P->CameraArm->SetRelativeRotation(FRotator(-50,-90,0));
    int I=0;for(auto Other:Mode->Fighters)if(Other!=P){Other->ResetForRound(FVector(Slot==1?160:420,I++*850,0));Other->Protection=1000;Other->MoveInput=FVector::ZeroVector;}
    FTimerHandle Handle;
    GetWorldTimerManager().SetTimer(Handle,FTimerDelegate::CreateWeakLambda(this,[this,Slot](){if(auto Pawn=Fighter()){
        if(auto M=GetWorld()->GetAuthGameMode<AWarlockMode>()){int Index=0;for(auto Other:M->Fighters)if(Other!=Pawn){Other->ResetForRound(FVector(Slot==1?160:420,Index++*850,0));Other->Protection=1000;}}
        Pawn->ResetForRound(FVector::ZeroVector);Pawn->Protection=0;Pawn->AimPoint=Pawn->GetActorLocation()+FVector(450,0,0);Pawn->Cast(Slot);
    }}),2.f,true,.2f);
#endif
}
void AWarlockController::ToggleHelp(){bHelpOpen=!bHelpOpen;}
void AWarlockController::Zoom(const FInputActionValue& Value){float Step=Value.Get<float>();if(FMath::IsNearlyZero(Step))return;if(auto P=Fighter();P&&!bShopOpen&&!bHelpOpen)P->CameraDistance=FMath::Clamp(P->CameraDistance-Step*130.f,1000.f,3200.f);}
void AWarlockController::CycleAppearance(){ServerAppearance();}
void AWarlockController::ServerAppearance_Implementation()
{
    auto Match=GetWorld()->GetGameState<AWarlockMatchState>();auto S=GetPlayerState<AWarlockState>();
    if(!S||!Match||Match->Phase!=TEXT("SHOP")){ClientNotice(TEXT("准备阶段按 H / 左摇杆切换外观"));return;}
    S->Appearance=(S->Appearance+1)%3;S->ForceNetUpdate();
    const TCHAR* Names[]={TEXT("赤曜术士"),TEXT("霜月术士"),TEXT("幽林术士")};ClientNotice(Names[S->Appearance]);
}
void AWarlockController::CycleResolution()
{
    if(GIsEditor){SetNotice(TEXT("分辨率设置在独立游戏中使用；滚轮可以调整镜头"));return;}
    EditDisplaySetting(1,1);
}
void AWarlockController::CycleQuality()
{
    EditDisplaySetting(0,1);
}
void AWarlockController::EditDisplaySetting(int32 Row,int32 Direction)
{
    if(auto Settings=Cast<UWarlockUserSettings>(UGameUserSettings::GetGameUserSettings()))
    {Settings->Adjust(Row,Direction);SetNotice(FString(UWarlockUserSettings::RowName(Row))+TEXT(" · ")+Settings->Label(Row));}
}
void AWarlockController::SetGraphicsPreset(int32 Preset)
{
    if(auto Settings=Cast<UWarlockUserSettings>(UGameUserSettings::GetGameUserSettings()))
    {Settings->SelectPreset(Preset);Settings->bClientSettingsInitialized=true;Settings->ApplyNonResolutionSettings();Settings->SaveSettings();}
}
void AWarlockController::ToggleDisplaySettings()
{
    OpenPausePage(bDisplaySettingsOpen?0:1);
}
void AWarlockController::OpenPausePage(int32 Page)
{
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)if(auto C=Cast<AWarlockController>(It->Get());C&&C->IsLocalController())
    {C->bDisplaySettingsOpen=Page==1;C->bPlayersMenuOpen=Page==2;C->bKeybindMenuOpen=Page==3;C->WaitingBinding=-1;}
}
void AWarlockController::ConfigureLocalPlayers(int32 Count)
{if(auto V=Cast<UWarlockViewport>(GetWorld()->GetGameViewport()))V->StartLocalSetup(Count);}
void AWarlockController::SetParticipantCount(int32 Count)
{
    auto First=Cast<AWarlockController>(GetWorld()->GetFirstPlayerController());if(First&&First!=this){First->SetParticipantCount(Count);return;}
    auto Mode=GetWorld()->GetAuthGameMode<AWarlockMode>();
    if(!Mode||!IsLocalController()||GetNetMode()!=NM_Standalone||!UGameplayStatics::IsGamePaused(this))return;
    if(auto View=Cast<UWarlockViewport>(GetWorld()->GetGameViewport());View&&View->bAssigningDevices)return;
    int Minimum=FMath::Max(2,Mode->GetNumPlayers()+(Mode->Match()->bTeams?1:0));
    if(Count<Minimum||Count>8||Count==Mode->Match()->ParticipantCount)return;
    Mode->Match()->ParticipantCount=Count;Mode->EnsureRoster();Mode->RestartMatch(Mode->Match()->bTeams);Mode->Match()->ForceNetUpdate();
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)if(auto C=Cast<AWarlockController>(It->Get());C&&C->IsLocalController())C->FlushLocalControls();
    GConfig->SetInt(TEXT("WarlockRoster"),TEXT("Participants"),Count,GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);
    UGameplayStatics::SetGamePaused(this,true);
}
void AWarlockController::FlushLocalControls()
{
    CancelTargeting();LocalMove=FVector::ZeroVector;bGamepadAiming=!UsesMouse();if(PlayerInput)PlayerInput->FlushPressedKeys();
    if(auto P=Fighter()){P->MoveInput=FVector::ZeroVector;P->bClickMoving=false;P->CancelCast();}
    if(auto Sub=LocalInput())Sub->RequestRebuildControlMappings(FModifyContextOptions(),EInputMappingRebuildType::RebuildWithFlush);
}
void AWarlockController::RefreshKeyboardMappings()
{
    if(!Mapping)return;
    auto Old=Mapping->GetMappings();
    for(auto& Entry:Old)if(Entry.Action&&!Entry.Key.IsGamepadKey()&&!Entry.Key.IsMouseButton())
        for(int I=0;I<UWarlockUserSettings::BindingCount;++I)if(Entry.Action->GetFName()==FName(UWarlockUserSettings::ActionName(I)))
        {Mapping->UnmapKey(Entry.Action,Entry.Key);Mapping->MapKey(Entry.Action,BindingKey(I));break;}
    FlushLocalControls();
}
void AWarlockController::StartKeyCapture(int32 Index)
{
    auto First=Cast<AWarlockController>(GetWorld()->GetFirstPlayerController());if(!First||!IsMenuOpen())return;
    First->SelectedBinding=FMath::Clamp(Index,0,UWarlockUserSettings::BindingCount-1);First->WaitingBinding=First->SelectedBinding;
    First->Notice.Empty();
}
void AWarlockController::ResetKeys()
{
    if(!IsMenuOpen())return;
    if(auto S=Cast<UWarlockUserSettings>(UGameUserSettings::GetGameUserSettings()))S->ResetKeyboardBindings();
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)if(auto C=Cast<AWarlockController>(It->Get());C&&C->IsLocalController()){C->WaitingBinding=-1;C->RefreshKeyboardMappings();}
}
void AWarlockController::NavigateShop(int Direction)
{
    if(!bShopOpen)return;
    if(SelectedProduct>=12&&SelectedProduct<20){SelectedProduct=12+(SelectedProduct-12+Direction+8)%8;return;}
    if(FMath::Abs(Direction)==1){AlternativeSelected();return;}
    int Family=WarlockCatalog::FromProduct(SelectedProduct);int Row=Family>=0?WarlockCatalog::Slot(Family):SelectedProduct;SelectShopProduct((Row+(Direction<0?-1:1)+12)%12);
}
void AWarlockController::ConfirmShop()
{
    if(bHelpOpen){bHelpOpen=false;return;}
    if(bShopOpen){ServerBuy(SelectedProduct);return;}
    if(auto M=GetWorld()->GetGameState<AWarlockMatchState>();M&&M->Phase==TEXT("FINAL"))ServerRestart(M->bTeams);
    else if(M&&M->Phase==TEXT("SHOP"))ToggleShop();
    else bHelpOpen=true;
}
void AWarlockController::AddLocalPlayer()
{
    auto M=GetWorld()->GetGameState<AWarlockMatchState>();if(!M||M->Phase!=TEXT("SHOP")||!HasAuthority()){SetNotice(TEXT("请在房主的准备阶段加入本地玩家"));return;}
    UGameMapsSettings::GetGameMapsSettings()->SetSkipAssigningGamepadToPlayer1(false);
    if(GetGameInstance()->GetNumLocalPlayers()<2)UGameplayStatics::CreatePlayer(this,-1,true);
    ApplyLocalRoles(3,3);
    SetNotice(TEXT("双手柄模式：第一只控制 P1，第二只控制 P2"));
}
void AWarlockController::AddMixedPlayer()
{
    auto M=GetWorld()->GetGameState<AWarlockMatchState>();if(!M||M->Phase!=TEXT("SHOP")||!HasAuthority())return;
    UGameMapsSettings::GetGameMapsSettings()->SetSkipAssigningGamepadToPlayer1(true);
    if(GetGameInstance()->GetNumLocalPlayers()<2)UGameplayStatics::CreatePlayer(this,-1,true);
    ApplyLocalRoles(4,3);
    SetNotice(TEXT("混合模式：键鼠控制 P1，第一只手柄控制 P2"));
}
void AWarlockController::ApplyLocalRoles(int FirstRole,int SecondRole)
{
    for(int I=0;I<2;++I)if(auto C=Cast<AWarlockController>(UGameplayStatics::GetPlayerController(this,I))){C->LocalControl=I?SecondRole:FirstRole;C->LocalPadIndex=I&&FirstRole==3?1:0;C->FlushLocalControls();}
}
void AWarlockController::AddKeyboardMousePlayer()
{
    auto M=GetWorld()->GetGameState<AWarlockMatchState>();if(!M||M->Phase!=TEXT("SHOP")||!HasAuthority())return;
    UGameMapsSettings::GetGameMapsSettings()->SetSkipAssigningGamepadToPlayer1(false);
    if(GetGameInstance()->GetNumLocalPlayers()<2)UGameplayStatics::CreatePlayer(this,-1,true);
    ApplyLocalRoles(1,2);SetNotice(TEXT("P1：方向键 / 技能键；P2：右键移动、左键施法、滚轮选技能、中键取消"));
}
void AWarlockController::ToggleTeams(){if(auto M=GetWorld()->GetGameState<AWarlockMatchState>();M&&M->Phase==TEXT("SHOP"))ServerRestart(!M->bTeams);}
void AWarlockController::ToggleSplitLayout()
{
    auto Settings=UGameMapsSettings::GetGameMapsSettings();Settings->TwoPlayerSplitscreenLayout=Settings->TwoPlayerSplitscreenLayout==ETwoPlayerSplitScreenType::Horizontal?ETwoPlayerSplitScreenType::Vertical:ETwoPlayerSplitScreenType::Horizontal;
    if(auto View=GetWorld()->GetGameViewport())View->LayoutPlayers();
}
void AWarlockController::ToggleMode(){if(bShopOpen)ServerShopMode(SelectedProduct);else ServerMode();}
void AWarlockController::SellSelected(){if(bShopOpen)ServerSell(SelectedProduct);}
void AWarlockController::StoneSelected(){if(bShopOpen)ServerStone(SelectedProduct);}
void AWarlockController::AlternativeSelected(){if(!bShopOpen)return;int F=WarlockCatalog::FromProduct(SelectedProduct);if(F<0)return;auto Options=WarlockCatalog::Families(WarlockCatalog::Slot(F));int I=Options.IndexOfByKey(F);SelectShopProduct(WarlockCatalog::Product(Options[(I+1)%Options.Num()]));}
void AWarlockController::SetNotice(const FString& Text){Notice=Text;NoticeTime=4;}
void AWarlockController::ServerInput_Implementation(FVector_NetQuantize Direction,FVector_NetQuantize Target)
{
    if(Direction.ContainsNaN()||Target.ContainsNaN())return;
    if(auto P=Fighter()){P->MoveInput=FVector(Direction.X,Direction.Y,0).GetClampedToMaxSize(1);P->AimPoint=FVector(Target.X,Target.Y,40).GetClampedToMaxSize2D(5000);if(!P->MoveInput.IsNearlyZero())P->bClickMoving=false;}
}
void AWarlockController::ServerClick_Implementation(FVector_NetQuantize Target)
{
    if(Target.ContainsNaN()||UGameplayStatics::IsGamePaused(this))return;if(auto P=Fighter()){P->FormMirrors(Target);P->CancelCast();P->ClickTarget=FVector(Target.X,Target.Y,40).GetClampedToMaxSize2D(5000);P->bClickMoving=true;}
}
void AWarlockController::ServerCast_Implementation(int Slot,FVector_NetQuantize Target)
{
    if(Target.ContainsNaN()||UGameplayStatics::IsGamePaused(this)||bShopOpen||bHelpOpen)return;if(auto P=Fighter()){P->AimPoint=FVector(Target.X,Target.Y,40).GetClampedToMaxSize2D(5000);if(!P->Cast(Slot))ClientNotice(TEXT("技能不可用：正在施法、尚未学习、冷却中或不在战斗阶段"));}
}
void AWarlockController::ServerBuy_Implementation(int Product){if(auto M=GetWorld()->GetAuthGameMode<AWarlockMode>()){FString Reason;M->Buy(Fighter(),Product,Reason);ClientNotice(Reason);}}
void AWarlockController::ServerMode_Implementation(){ClientNotice(TEXT("准备阶段打开商店，选中技能后按 M / X 免费切换模式"));}
void AWarlockController::ServerShopMode_Implementation(int Slot){if(auto M=GetWorld()->GetAuthGameMode<AWarlockMode>()){FString R;M->SetSkillMode(Fighter(),Slot,R);ClientNotice(R);}}
void AWarlockController::ServerSell_Implementation(int Product){if(auto M=GetWorld()->GetAuthGameMode<AWarlockMode>()){FString R;M->Sell(Fighter(),Product,R);ClientNotice(R);}}
void AWarlockController::ServerStone_Implementation(int Slot)
{
    auto P=Fighter();auto S=P?P->State():nullptr;
    if(S&&(Slot==6||Slot==20)&&S->DefenseChoice!=(Slot==20?1:0)){ClientNotice(TEXT("请选中本场已学习的 C 槽路线再启用石头"));return;}
    if(auto M=GetWorld()->GetAuthGameMode<AWarlockMode>()){FString R;M->AssignStone(P,Slot,R);ClientNotice(R);}
}
void AWarlockController::ServerRestart_Implementation(bool Teams)
{
    if(auto M=GetWorld()->GetAuthGameMode<AWarlockMode>())
    {
        if(M->Match()->bOnlineRoom){if(M->Match()->Phase==TEXT("FINAL"))ServerReturnLobby();}
        else if(GetLocalPlayer())M->RestartMatch(Teams);
    }
}
void AWarlockController::ClientNotice_Implementation(const FString& Text){SetNotice(Text);}

AWarlockAI::AWarlockAI(){bWantsPlayerState=true;}
void AWarlockAI::BeginPlay(){Super::BeginPlay();Random.Initialize(GetUniqueID()*7919);}
void AWarlockAI::ResetPerception(){ObservedTarget.Reset();ObservedBlinkSerial=-1;ReactionRemaining=0;NextDecision=0;bObservedThreat=false;bHasObservation=false;ObservedVelocity=FVector::ZeroVector;Tactic=0;TacticWait=FeintCooldown=ComboCooldown=0;StrafeTimer=0;ExpertFrames.Reset();ExpertView={};ExpertClock=ExpertNextCapture=ExpertNextAction=ExpertNextMode=0;ExpertLastTarget.Reset();ExpertLastCapture=0;ExpertReactionAge=0;ExpertMinimumReaction=100;ExpertDodgeUntil=0;ExpertDodgeDirection=FVector::ZeroVector;ExpertExperience={};}
void AWarlockAI::Shop()
{
    auto P=Cast<AWarlockPawn>(GetPawn());auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();if(!P||!P->State()||!M)return;
    FString Reason;
    if((DifficultyOverride?DifficultyOverride:M->Match()->AIDifficulty)==6){ShopExpert();return;}
    if(ShopPlanOverride<0&&!M->bTraining)
    {
        // Same eight action roles for the policy, with different actual
        // families. Preserve explicit training curricula below.
        static const int Routes[][6]={{2,3,4,5,6,7},{9,11,13,16,18,21},{10,12,14,17,19,20},{2,3,13,15,8,22},{9,11,4,16,6,7},{10,3,14,5,18,21}};
        auto S=P->State();auto& Route=Routes[FMath::Abs(S->ColorIndex)%6];
        for(int Slot:{3,2,4,5,6,7})if(S->Level(Slot)==0){int Product=WarlockCatalog::Product(Route[Slot-2]);int Cost=M->Price(P,Product);if(Cost>0&&Cost<=S->Gold)M->Buy(P,Product,Reason);}
        // Bank for a useful second/third family instead of spending every
        // ten-coin supply on the first mobility skill indefinitely.
        if(S->LearnedCount()<3)return;
        for(int Pass=0;Pass<2;++Pass)
        {
            for(int Research:{8,9,10}){int Cost=M->Price(P,Research);if(Cost>0&&Cost<=S->Gold)M->Buy(P,Research,Reason);}
            for(int Slot:{0,2,4,5,6,7,3}){int Product=WarlockCatalog::Product(WarlockCatalog::Family(S,Slot));int Cost=M->Price(P,Product);if(Cost>0&&Cost<=S->Gold)M->Buy(P,Product,Reason);}
        }
        return;
    }
    const int Plans[][7]={{3,2,4,8,9,10,0},{2,3,5,8,10,9,0},{4,3,6,9,8,10,0}};
    auto& Plan=Plans[ShopPlanOverride>=0?ShopPlanOverride%3:GetUniqueID()%3];
    for(int Pass=0;Pass<2;++Pass)for(int I:Plan){int Price=M->Price(P,I);if(Price>0&&Price<=P->State()->Gold)M->Buy(P,I,Reason);}
}
void AWarlockAI::Think()
{
    if(auto State=GetWorld()->GetGameState<AWarlockMatchState>();(DifficultyOverride?DifficultyOverride:State?State->AIDifficulty:3)==6)return;
    auto P=Cast<AWarlockPawn>(GetPawn());auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();if(!P||!P->State()||!P->bAlive||!M)return;
    if(Tactic!=0)return;
    auto Position=P->GetActorLocation();AWarlockPawn* Target=nullptr;float Best=MAX_flt;
    for(auto Other:M->Fighters)if(Other->bAlive&&P->IsEnemy(Other)&&(Other->StealthRemaining<=0||FVector::Dist2D(Position,Other->GetActorLocation())<=100)){float D=FVector::DistSquared2D(Position,Other->GetActorLocation());if(D<Best){Target=Other;Best=D;}}
    if(!Target)return;
    const auto Tuning=GetDefault<UWarlockTuning>();
    if(ObservedTarget!=Target||ObservedBlinkSerial!=Target->BlinkSerial)
    {
        ObservedTarget=Target;ObservedBlinkSerial=Target->BlinkSerial;
        bHasObservation=false;
        auto Profile=Difficulty();ReactionRemaining=Random.FRandRange(Profile.ReactionMin,Profile.ReactionMax);
        P->MoveInput=FVector::ZeroVector;return;
    }
    ReactionRemaining=FMath::Max(0.f,ReactionRemaining-.22f);
    if(ReactionRemaining>0)return;
    float ObservationTime=DecisionTime();FVector NewPosition=Target->GetActorLocation();
    ObservedVelocity=bHasObservation?(NewPosition-ObservedTargetPosition)/FMath::Max(.22f,ObservationTime-LastObservationTime):FVector::ZeroVector;ObservedVelocity=ObservedVelocity.GetClampedToMaxSize2D(600);
    ObservedTargetPosition=NewPosition;ObservedMomentum=Target->Momentum;LastObservationTime=ObservationTime;bHasObservation=true;
    auto D=(ObservedTargetPosition-Position).GetSafeNormal2D();float Distance=FVector::Dist2D(ObservedTargetPosition,Position);
    auto Profile=Difficulty();P->AimPoint=ObservedTargetPosition+ObservedVelocity*FMath::Min(.4f,Distance/600.f)+FVector(Random.FRandRange(-Profile.AimError,Profile.AimError),Random.FRandRange(-Profile.AimError,Profile.AimError),0);
    bool Edge=Position.Size2D()>M->Match()->ArenaRadius-150;
    FVector Move=Edge?-Position.GetSafeNormal2D():D*(Distance>620?1:Distance<330?-1:0)+FVector(-D.Y,D.X,0)*StrafeSign*.6f;
    if(M->Arena)for(int I=0;I<M->Arena->Pillars.Num();++I)if(M->Arena->PillarHealth[I]>0)
    {FVector V=Position-M->Arena->Pillars[I];if(V.Size2D()<180)Move+=V.GetSafeNormal2D()*1.5f;}
    bool Threat=false;
    for(TActorIterator<AWarlockProjectile> It(GetWorld());It;++It)if(It->Caster&&It->Caster->IsEnemy(P)&&FVector::Dist2D(Position,It->GetActorLocation())<350)
    {float V2=It->Velocity.SizeSquared2D();float Ahead=V2>1?FVector::DotProduct(Position-It->GetActorLocation(),It->Velocity)/V2:-1;
        if(Ahead<0||Ahead>.6f||FVector::Dist2D(Position,It->GetActorLocation()+It->Velocity*Ahead)>It->Radius+70)continue;
        Threat=true;auto Side=FVector(-It->Velocity.Y,It->Velocity.X,0).GetSafeNormal();Move+=Side*(FVector::DotProduct(Position-It->GetActorLocation(),Side)>0?1:-1);}
    if(Threat&&!bObservedThreat){bObservedThreat=true;ReactionRemaining=Random.FRandRange(Profile.ReactionMin,Profile.ReactionMax);return;}
    bObservedThreat=Threat;
    // The next observation is delayed too; no per-frame reads during this window.
    ReactionRemaining=Random.FRandRange(Profile.ReactionMin,Profile.ReactionMax);
    P->MoveInput=Move.GetClampedToMaxSize(1);P->bClickMoving=false;
    float Now=DecisionTime();if(P->PendingCast>=0||Now<NextDecision)return;
    if(Random.FRand()<Profile.SkipChance){NextDecision=Now+Profile.Interval;return;}
    if(P->Protection<=0&&(bLearning||Random.FRand()<Profile.PolicyChance)&&ChoosePolicyAction())return;
    if(!Edge&&P->Protection<=0&&Profile.PolicyChance>=.6f){if(Random.FRand()<.26f&&TryBlinkSmite())return;if(Random.FRand()<.16f&&TryFeint())return;}
    bool Casted=false;
    if(Edge&&P->State()->Level(3)>0){P->AimPoint=Position-Position.GetSafeNormal2D()*700;Casted=P->Cast(3);}
    else if(P->Protection<=0){
        if(P->Knockback>25)Casted=P->Cast(6);
        if(!Casted&&Distance<240)Casted=P->Cast(1);
        if(!Casted&&Distance<600)Casted=P->Cast(2);
        if(!Casted&&Distance<800)Casted=P->Cast(4);
        if(!Casted&&Distance<800&&ObservedMomentum.Size2D()<10)Casted=P->Cast(5);
        if(!Casted&&Distance<950)Casted=P->Cast(0);
        if(!Casted&&!Edge&&Distance>500)Casted=P->Cast(7);
    }
    if(Casted)NextDecision=Now+Profile.Interval;
}
