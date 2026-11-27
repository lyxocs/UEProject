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
    if(!IsLocalController()||FPlatformTime::Seconds()<NextNetworkReport)return;
    float ReportPeriod=.1f;FParse::Value(FCommandLine::Get(),TEXT("WarlockNetTestPeriod="),ReportPeriod);NextNetworkReport=FPlatformTime::Seconds()+FMath::Clamp(ReportPeriod,.1f,2.f);
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
            else if(Action==TEXT("room"))ServerRoomOptions(Value,Command->HasField(TEXT("difficulty"))?Command->GetIntegerField(TEXT("difficulty")):3,false);else if(Action==TEXT("back"))ServerReturnLobby();
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
    Json->SetStringField(TEXT("phase"),M->Phase);Json->SetNumberField(TEXT("round"),M->Round);Json->SetNumberField(TEXT("remaining"),M->Remaining);Json->SetNumberField(TEXT("seats"),M->ParticipantCount);Json->SetNumberField(TEXT("difficulty"),M->AIDifficulty);
    Json->SetNumberField(TEXT("host"),M->RoomHostId);Json->SetStringField(TEXT("notice"),Notice);Json->SetBoolField(TEXT("online"),M->bOnlineRoom);Json->SetNumberField(TEXT("aiming"),AimingSlot);
    if(auto Self=GetPlayerState<AWarlockState>())Json->SetNumberField(TEXT("self"),Self->GetPlayerId());
    TArray<TSharedPtr<FJsonValue>> Roster;
    for(TActorIterator<AWarlockPawn> It(GetWorld());It;++It)if(auto S=It->State())
    {
        auto P=*It;auto Entry=MakeShared<FJsonObject>();Entry->SetNumberField(TEXT("id"),S->GetPlayerId());Entry->SetStringField(TEXT("name"),S->GetPlayerName());Entry->SetBoolField(TEXT("bot"),S->bBot);Entry->SetBoolField(TEXT("ready"),S->bReady);
        Entry->SetNumberField(TEXT("gold"),S->Gold);Entry->SetNumberField(TEXT("health"),P->Health);Entry->SetNumberField(TEXT("knockback"),P->Knockback);Entry->SetBoolField(TEXT("alive"),P->bAlive);
        Entry->SetNumberField(TEXT("x"),P->GetActorLocation().X);Entry->SetNumberField(TEXT("y"),P->GetActorLocation().Y);Entry->SetNumberField(TEXT("cast"),P->CastSerial);Entry->SetNumberField(TEXT("pending"),P->PendingCast);
        TArray<TSharedPtr<FJsonValue>> Levels;for(int L:S->Levels)Levels.Add(MakeShared<FJsonValueNumber>(L));Entry->SetArrayField(TEXT("levels"),Levels);
        TArray<TSharedPtr<FJsonValue>> Families;for(int F:S->Families)Families.Add(MakeShared<FJsonValueNumber>(F));Entry->SetArrayField(TEXT("families"),Families);
        Entry->SetNumberField(TEXT("placement"),S->RoundPlacement);Entry->SetNumberField(TEXT("kill_income"),S->KillIncome);Entry->SetNumberField(TEXT("assist_income"),S->AssistIncome);Entry->SetNumberField(TEXT("victory_income"),S->VictoryIncome);Entry->SetNumberField(TEXT("damage_income"),S->DamageIncome);Entry->SetNumberField(TEXT("supply_income"),S->SupplyIncome);
        Entry->SetNumberField(TEXT("root"),P->RootRemaining);Entry->SetNumberField(TEXT("disrupt"),P->DisruptRemaining);Entry->SetNumberField(TEXT("rewind"),P->RewindRemaining);Entry->SetNumberField(TEXT("mirror"),P->MirrorRemaining);
        if(auto AI=Cast<AWarlockAI>(P->GetController())){Entry->SetNumberField(TEXT("expert_decisions"),AI->ExpertDecisions);Entry->SetNumberField(TEXT("expert_minimum_reaction"),AI->ExpertMinimumReaction);Entry->SetNumberField(TEXT("expert_items"),AI->ExpertItemPurchases);}
        Roster.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Json->SetArrayField(TEXT("roster"),Roster);FString Output;
    Json->SetBoolField(TEXT("expert_model_loaded"),FWarlockExpertPolicy::Shared().bReady);
    TArray<TSharedPtr<FJsonValue>> Pillars;for(TActorIterator<AWarlockArena> It(GetWorld());It;++It)for(auto Position:It->Pillars){auto Item=MakeShared<FJsonObject>();Item->SetNumberField(TEXT("x"),Position.X);Item->SetNumberField(TEXT("y"),Position.Y);Pillars.Add(MakeShared<FJsonValueObject>(Item));}Json->SetArrayField(TEXT("pillars"),Pillars);
    TArray<TSharedPtr<FJsonValue>> Rewards;for(TActorIterator<AWarlockRewardFX> It(GetWorld());It;++It)if(It->Source&&It->Source->State()){auto Item=MakeShared<FJsonObject>();Item->SetNumberField(TEXT("source"),It->Source->State()->GetPlayerId());Item->SetNumberField(TEXT("kind"),It->Kind);Item->SetNumberField(TEXT("gold"),It->Gold);Item->SetNumberField(TEXT("points"),It->Points);Item->SetNumberField(TEXT("streak"),It->Streak);Rewards.Add(MakeShared<FJsonValueObject>(Item));}Json->SetArrayField(TEXT("rewards"),Rewards);
    FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&Output));
    FFileHelper::SaveStringToFile(Output,*(Prefix+TEXT(".json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
#endif
}
