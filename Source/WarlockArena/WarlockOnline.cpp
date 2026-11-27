#include "WarlockOnline.h"
#include "WarlockGame.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/NetworkVersion.h"

void UWarlockOnline::Init()
{
    Super::Init();
    // Launcher clients and the exact Preview-2 source build have different build
    // changelists. Pin a project protocol, not a global compatibility bypass.
    FNetworkVersion::GetLocalNetworkVersionOverride.BindLambda([](){return uint32(0x574C1002);});
    GConfig->GetString(TEXT("WarlockOnline"),TEXT("Address"),Address,GGameUserSettingsIni);
    GConfig->GetString(TEXT("WarlockOnline"),TEXT("Name"),DisplayName,GGameUserSettingsIni);
    NetworkFailureHandle=GEngine->OnNetworkFailure().AddUObject(this,&UWarlockOnline::NetworkFailure);
    TravelFailureHandle=GEngine->OnTravelFailure().AddUObject(this,&UWarlockOnline::TravelFailure);
}
void UWarlockOnline::Shutdown()
{
    CloseConnectionPanel();GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    Super::Shutdown();
}
bool UWarlockOnline::ValidEndpoint(const FString& Value)
{
    FString Host,Port;if(!Value.Split(TEXT(":"),&Host,&Port)||Host.IsEmpty()||Port.IsEmpty()||Value.Len()>253)return false;
    for(TCHAR C:Host)if(!FChar::IsAlnum(C)&&C!='.'&&C!='-')return false;
    for(TCHAR C:Port)if(!FChar::IsDigit(C))return false;
    return FCString::Atoi(*Port)>0&&FCString::Atoi(*Port)<=65535;
}
void UWarlockOnline::JoinRoom(const FString& Endpoint,const FString& Name)
{
    FString Clean=Endpoint.TrimStartAndEnd();if(!ValidEndpoint(Clean)){Status=TEXT("请输入服务器地址，例如 21.6.1.43:7777");return;}
    Address=Clean;DisplayName=Name.TrimStartAndEnd().Left(24);if(DisplayName.IsEmpty())DisplayName=TEXT("Warlock");
    for(TCHAR& C:DisplayName)if(FChar::IsControl(C))C=' ';
    GConfig->SetString(TEXT("WarlockOnline"),TEXT("Address"),*Address,GGameUserSettingsIni);
    GConfig->SetString(TEXT("WarlockOnline"),TEXT("Name"),*DisplayName,GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);
    CloseConnectionPanel();Status=TEXT("正在连接服务器…");
    // This first online version has one seat per process; offline split-screen
    // remains available. Avoid carrying two unassigned controllers into a room.
    while(GetNumLocalPlayers()>1)RemoveLocalPlayer(GetLocalPlayerByIndex(GetNumLocalPlayers()-1));
    if(auto C=GetFirstLocalPlayerController()){UGameplayStatics::SetGamePaused(this,false);C->ClientTravel(Address,TRAVEL_Absolute);}
}
void UWarlockOnline::LeaveRoom()
{
    CloseConnectionPanel();UGameplayStatics::SetGamePaused(this,false);
    while(GetNumLocalPlayers()>1)RemoveLocalPlayer(GetLocalPlayerByIndex(GetNumLocalPlayers()-1));
    UGameplayStatics::OpenLevel(this,TEXT("/Game/WarlockArena/Maps/Arena"));
}
void UWarlockOnline::NetworkFailure(UWorld*,UNetDriver*,ENetworkFailure::Type,const FString& Error)
{
    Status=TEXT("连接已断开，请检查服务器地址与网络后重试。");
    if(Error.Contains(TEXT("room is full")))Status=TEXT("连接已断开：房间已满，请稍后重试。");
    if(Error.Contains(TEXT("Join between rounds")))Status=TEXT("连接已断开：服务器正在战斗，请在回合准备或大厅阶段重试。");
    UE_LOG(LogTemp,Warning,TEXT("WarlockOnline: %s"),*Error);
}
void UWarlockOnline::TravelFailure(UWorld*,ETravelFailure::Type,const FString& Error)
{Status=TEXT("无法进入房间，请确认客户端版本与服务器一致后重试。");UE_LOG(LogTemp,Warning,TEXT("WarlockOnline: %s"),*Error);}
void UWarlockOnline::CloseConnectionPanel()
{
    if(Panel.IsValid()&&GetGameViewportClient())GetGameViewportClient()->RemoveViewportWidgetContent(Panel.ToSharedRef());Panel.Reset();
    if(auto C=Cast<AWarlockController>(GetFirstLocalPlayerController())){C->bConnectionPanelOpen=false;C->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));}
}
void UWarlockOnline::ShowConnectionPanel()
{
    if(!GetGameViewportClient())return;if(Panel.IsValid()){CloseConnectionPanel();return;}
    if(auto C=Cast<AWarlockController>(GetFirstLocalPlayerController())){C->CancelTargeting();C->ServerStop();C->bConnectionPanelOpen=true;C->SetInputMode(FInputModeUIOnly());}
    auto Endpoint=SNew(SEditableTextBox).Text(FText::FromString(Address));
    auto Name=SNew(SEditableTextBox).Text(FText::FromString(DisplayName));
    Panel=SNew(SBorder).HAlign(HAlign_Center).VAlign(VAlign_Center).BorderBackgroundColor(FLinearColor(.008f,.015f,.025f,.97f))
    [SNew(SBox).WidthOverride(540)[SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(16)[SNew(STextBlock).Text(FText::FromString(TEXT("术士之战 · 联机大厅"))).Font(FCoreStyle::GetDefaultFontStyle("Bold",25))]
        +SVerticalBox::Slot().AutoHeight().Padding(16,6)[SNew(STextBlock).Text(FText::FromString(TEXT("专用服务器地址（游戏 UDP 端口）")))]
        +SVerticalBox::Slot().AutoHeight().Padding(16,6)[Endpoint]
        +SVerticalBox::Slot().AutoHeight().Padding(16,6)[SNew(STextBlock).Text(FText::FromString(TEXT("玩家名称")))]
        +SVerticalBox::Slot().AutoHeight().Padding(16,6)[Name]
        +SVerticalBox::Slot().AutoHeight().Padding(16,12)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(Status.IsEmpty()?TEXT("2～8席 · AI补位 · 全员准备后进入回合商店"):Status);})]
        +SVerticalBox::Slot().AutoHeight().Padding(16,6)[SNew(SButton).Text(FText::FromString(TEXT("连接房间"))).OnClicked_Lambda([this,Endpoint,Name](){JoinRoom(Endpoint->GetText().ToString(),Name->GetText().ToString());return FReply::Handled();})]
        +SVerticalBox::Slot().AutoHeight().Padding(16,6)[SNew(SButton).Text(FText::FromString(TEXT("返回游戏"))).OnClicked_Lambda([this](){CloseConnectionPanel();return FReply::Handled();})]
    ]];
    GetGameViewportClient()->AddViewportWidgetContent(Panel.ToSharedRef(),100);
}

void AWarlockController::OpenOnlineMenu(){if(auto GI=GetGameInstance<UWarlockOnline>())GI->ShowConnectionPanel();}
void AWarlockController::JoinServer(const FString& Endpoint,const FString& Name){if(auto GI=GetGameInstance<UWarlockOnline>())GI->JoinRoom(Endpoint,Name);}
void AWarlockController::LeaveServer(){if(auto GI=GetGameInstance<UWarlockOnline>())GI->LeaveRoom();}
void AWarlockController::ServerRoomOptions_Implementation(int Participants,int Difficulty,bool Teams)
{
    auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();if(!M||!M->Match()||M->Match()->Phase!=TEXT("LOBBY")||GetPlayerState<AWarlockState>()->GetPlayerId()!=M->Match()->RoomHostId)return;
    if(Participants<FMath::Max(2,M->GetNumPlayers())||Participants>8||Difficulty<1||Difficulty>6)return;
    M->Match()->ParticipantCount=Participants;M->Match()->AIDifficulty=Difficulty;M->Match()->bTeams=Teams;
    M->EnsureRoster();M->Match()->ForceNetUpdate();
}
void AWarlockController::ServerDisplayName_Implementation(const FString& Name)
{
    if(auto S=GetPlayerState<AWarlockState>()){FString Clean=Name.TrimStartAndEnd().Left(24);for(TCHAR& C:Clean)if(FChar::IsControl(C))C=' ';if(!Clean.IsEmpty())S->SetPlayerName(Clean);S->ForceNetUpdate();}
}
void AWarlockController::ServerReturnLobby_Implementation()
{
    auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();auto S=GetPlayerState<AWarlockState>();
    if(!M||!S||!M->Match()->bOnlineRoom||S->GetPlayerId()!=M->Match()->RoomHostId||M->Match()->Phase==TEXT("FIGHT"))return;
    M->RestartMatch(M->Match()->bTeams);M->Match()->Phase=TEXT("LOBBY");M->Match()->Remaining=0;M->EnsureRoster();
}
