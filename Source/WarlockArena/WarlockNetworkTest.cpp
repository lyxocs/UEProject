#include "WarlockGame.h"
#include "WarlockOnline.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

// Opt-in Development harness, using the same owned RPCs as real input. Never
// enabled by a network request and absent from Shipping. No combat cheats.
void AWarlockController::TickNetworkTest()
{
#if !UE_BUILD_SHIPPING
    if(!IsLocalController()||FPlatformTime::Seconds()<NextNetworkReport)return;NextNetworkReport=FPlatformTime::Seconds()+.1;
    FString Prefix;if(!FParse::Value(FCommandLine::Get(),TEXT("WarlockNetTest="),Prefix))return;
    auto Online=GetGameInstance<UWarlockOnline>();if(Online)LastNetworkCommand=Online->LastTestCommand;
    FString Text;TSharedPtr<FJsonObject> Command;
    if(FFileHelper::LoadFileToString(Text,*(Prefix+TEXT(".command.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Command)&&Command.IsValid())
    {
        int Id=Command->GetIntegerField(TEXT("id"));if(Id>LastNetworkCommand)
        {
            LastNetworkCommand=Id;if(Online)Online->LastTestCommand=Id;FString Action=Command->GetStringField(TEXT("action"));
            int Value=Command->HasField(TEXT("value"))?Command->GetIntegerField(TEXT("value")):0;
            FVector Target(Command->HasField(TEXT("x"))?Command->GetNumberField(TEXT("x")):400,Command->HasField(TEXT("y"))?Command->GetNumberField(TEXT("y")):0,40);
            if(Action==TEXT("ready"))ServerReady();else if(Action==TEXT("buy"))ServerBuy(Value);
            else if(Action==TEXT("cast"))ServerCast(Value,Target);else if(Action==TEXT("stop"))ServerStop();
            else if(Action==TEXT("move"))ServerInput(FVector(Target.X,Target.Y,0).GetClampedToMaxSize(1),Target);
            else if(Action==TEXT("click"))ServerClick(Target);else if(Action==TEXT("pause"))TogglePause();
            else if(Action==TEXT("room"))ServerRoomOptions(Value,3,false);else if(Action==TEXT("back"))ServerReturnLobby();
            else if(Action==TEXT("leave"))LeaveServer();else if(Action==TEXT("join"))JoinServer(Command->GetStringField(TEXT("address")),Command->GetStringField(TEXT("name")));
            else if(Action==TEXT("menu"))OpenOnlineMenu();else if(Action==TEXT("closemenu")){if(auto GI=GetGameInstance<UWarlockOnline>())GI->CloseConnectionPanel();}
            else if(Action==TEXT("aim")){LocalAim=Target;StartTargeting(Value);}else if(Action==TEXT("release"))ReleaseTargeting(Value);
            else if(Action==TEXT("capture"))ConsoleCommand(TEXT("CaptureScene ")+Command->GetStringField(TEXT("path"))+TEXT(" 0.2"),false);
            UE_LOG(LogTemp,Display,TEXT("WarlockNetTest: command %d %s"),Id,*Action);
        }
    }
    auto Json=MakeShared<FJsonObject>();Json->SetNumberField(TEXT("command"),LastNetworkCommand);
    Json->SetNumberField(TEXT("net_mode"),int(GetNetMode()));Json->SetBoolField(TEXT("menu"),IsMenuOpen());Json->SetBoolField(TEXT("world_paused"),UGameplayStatics::IsGamePaused(this));
    auto M=GetWorld()->GetGameState<AWarlockMatchState>();if(!M)return;
    Json->SetStringField(TEXT("phase"),M->Phase);Json->SetNumberField(TEXT("round"),M->Round);Json->SetNumberField(TEXT("remaining"),M->Remaining);Json->SetNumberField(TEXT("seats"),M->ParticipantCount);
    Json->SetNumberField(TEXT("host"),M->RoomHostId);Json->SetStringField(TEXT("notice"),Notice);Json->SetBoolField(TEXT("online"),M->bOnlineRoom);Json->SetNumberField(TEXT("aiming"),AimingSlot);
    if(auto Self=GetPlayerState<AWarlockState>())Json->SetNumberField(TEXT("self"),Self->GetPlayerId());
    TArray<TSharedPtr<FJsonValue>> Roster;
    for(TActorIterator<AWarlockPawn> It(GetWorld());It;++It)if(auto S=It->State())
    {
        auto P=*It;auto Entry=MakeShared<FJsonObject>();Entry->SetNumberField(TEXT("id"),S->GetPlayerId());Entry->SetStringField(TEXT("name"),S->GetPlayerName());Entry->SetBoolField(TEXT("bot"),S->bBot);Entry->SetBoolField(TEXT("ready"),S->bReady);
        Entry->SetNumberField(TEXT("gold"),S->Gold);Entry->SetNumberField(TEXT("health"),P->Health);Entry->SetNumberField(TEXT("knockback"),P->Knockback);Entry->SetBoolField(TEXT("alive"),P->bAlive);
        Entry->SetNumberField(TEXT("x"),P->GetActorLocation().X);Entry->SetNumberField(TEXT("y"),P->GetActorLocation().Y);Entry->SetNumberField(TEXT("cast"),P->CastSerial);Entry->SetNumberField(TEXT("pending"),P->PendingCast);
        TArray<TSharedPtr<FJsonValue>> Levels;for(int L:S->Levels)Levels.Add(MakeShared<FJsonValueNumber>(L));Entry->SetArrayField(TEXT("levels"),Levels);
        Roster.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Json->SetArrayField(TEXT("roster"),Roster);FString Output;FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&Output));
    FFileHelper::SaveStringToFile(Output,*(Prefix+TEXT(".json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
#endif
}
