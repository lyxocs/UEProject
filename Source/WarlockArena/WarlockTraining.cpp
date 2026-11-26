#include "WarlockGame.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

void AWarlockMode::ResetTrainingEpisode()
{
    bFinished=false;Accumulator=GroundAccumulator=BotThinkTimer=0;TrainingEpisodeTime=0;TrainingMeteors.Reset();
    Match()->bTeams=false;Match()->Phase=TEXT("SHOP");Match()->Round=0;
    for(int I=0;I<Fighters.Num();++I){auto P=Fighters[I];auto S=P->State();S->Team=I;S->bBot=true;S->Gold=20+10*(TrainingEpisodes%7);S->Points=0;
        S->Levels.Init(0,8);S->Levels[0]=S->Levels[1]=1;S->Modes.Init(false,8);S->Items.Init(0,8);S->LifeMastery=S->RangeMastery=S->TimeMastery=0;S->bHasStone=false;S->StoneSlot=-1;
        auto AI=CastChecked<AWarlockAI>(P->GetController());AI->Experience.Action=-1;AI->Random.Initialize(TrainingSeed+TrainingEpisodes*101+I*7919);AI->ShopPlanOverride=TrainingEpisodes%3;AI->Shop();}
    StartRound();FRandomStream Spawn(TrainingSeed+TrainingEpisodes*101);float Angle=Spawn.FRandRange(0.f,2*PI);
    for(int I=0;I<2;++I)Fighters[I]->ResetForRound(FVector(FMath::Cos(Angle+I*PI)*600,FMath::Sin(Angle+I*PI)*600,0));
}
void AWarlockMode::TrainingBatch()
{
    if(!bTrainingInitialized)
    {
        if(Fighters.Num()!=4||!GetWorld()->GetFirstPlayerController()||!GetWorld()->GetFirstPlayerController()->GetPawn())return;
        bTrainingInitialized=true;TrainingWallStart=FPlatformTime::Seconds();
        FParse::Value(FCommandLine::Get(),TEXT("TrainEpisodes="),TrainingLimit);TrainingLimit=FMath::Clamp(TrainingLimit,1,100000);
        FParse::Value(FCommandLine::Get(),TEXT("TrainSeed="),TrainingSeed);FParse::Value(FCommandLine::Get(),TEXT("TrainTier="),TrainingTier);TrainingTier=FMath::Clamp(TrainingTier,1,5);
        FParse::Value(FCommandLine::Get(),TEXT("TrainOutput="),TrainingOutput);if(TrainingOutput.IsEmpty())TrainingOutput=FPaths::ProjectSavedDir()/TEXT("Training/run");
        IFileManager::Get().MakeDirectory(*TrainingOutput,true);bEvaluation=FParse::Param(FCommandLine::Get(),TEXT("TrainEvaluate"));
        auto& Policy=FWarlockPolicy::Shared();FString Initial;if(FParse::Value(FCommandLine::Get(),TEXT("TrainPolicy="),Initial)&&!Policy.Load(Initial)){UE_LOG(LogTemp,Error,TEXT("Training policy could not be loaded"));FPlatformMisc::RequestExitWithStatus(false,2);return;}
        auto Player=GetWorld()->GetFirstPlayerController();auto Pawn=CastChecked<AWarlockPawn>(Player->GetPawn());Player->UnPossess();auto Learner=GetWorld()->SpawnActor<AWarlockAI>();Learner->Possess(Pawn);
        for(int I=Fighters.Num()-1;I>=2;--I){auto P=Fighters[I];auto C=P->GetController();P->Destroy();C->Destroy();Fighters.RemoveAt(I);}
        Learner->DifficultyOverride=TrainingTier;Learner->bLearning=!bEvaluation;Learner->bDisablePolicy=FParse::Param(FCommandLine::Get(),TEXT("TrainBaseline"));
        if(FParse::Param(FCommandLine::Get(),TEXT("TrainUntrained"))){Policy=FWarlockPolicy();Policy.bReady=true;}
        Policy.Save(TrainingOutput/TEXT("initial-policy.json"));
        auto Opponent=CastChecked<AWarlockAI>(Fighters[1]->GetController());Opponent->bDisablePolicy=true;Opponent->DifficultyOverride=3;
        ResetTrainingEpisode();UE_LOG(LogTemp,Display,TEXT("WARLOCK_RL started episodes=%d tier=%d eval=%d"),TrainingLimit,TrainingTier,bEvaluation);
    }
    // Fixed-size batches yield to UE between batches for actor cleanup and GC.
    bTrainingStep=true;
    for(int StepIndex=0;StepIndex<128;++StepIndex)
    {
        TrainingClock+=WarlockRules::Step;TrainingEpisodeTime+=WarlockRules::Step;Tick(WarlockRules::Step);
        if(Match()->Phase==TEXT("FIGHT")&&TrainingEpisodeTime<90)continue;
        auto Learner=CastChecked<AWarlockAI>(Fighters[0]->GetController());bool Win=Fighters[0]->bAlive&&!Fighters[1]->bAlive,Loss=!Fighters[0]->bAlive&&Fighters[1]->bAlive;
        Learner->LearnTransition(true,Win?2.f:Loss?-2.f:0.f);TrainingWins+=Win;TrainingLosses+=Loss;TrainingDraws+=!Win&&!Loss;++TrainingEpisodes;
        if(TrainingEpisodes%50==0){UE_LOG(LogTemp,Display,TEXT("WARLOCK_RL episodes=%d wins=%d losses=%d draws=%d updates=%d steps_per_second=%.0f"),TrainingEpisodes,TrainingWins,TrainingLosses,TrainingDraws,FWarlockPolicy::Shared().Updates,TrainingClock/WarlockRules::Step/(FPlatformTime::Seconds()-TrainingWallStart));
            if(!bEvaluation)FWarlockPolicy::Shared().Save(TrainingOutput/FString::Printf(TEXT("policy-%05d.json"),TrainingEpisodes));GEngine->ForceGarbageCollection(true);}
        float Budget=240;FParse::Value(FCommandLine::Get(),TEXT("TrainSeconds="),Budget);
        if(TrainingEpisodes>=TrainingLimit||(!bEvaluation&&FPlatformTime::Seconds()-TrainingWallStart>Budget)){FinishTraining();bTrainingStep=false;return;}
        ResetTrainingEpisode();
    }
    bTrainingStep=false;
}
void AWarlockMode::FinishTraining()
{
    auto& Policy=FWarlockPolicy::Shared();if(!bEvaluation)Policy.Save(TrainingOutput/TEXT("policy.json"));
    auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("algorithm"),TEXT("masked softmax actor-critic TD(0)"));Report->SetBoolField(TEXT("evaluation"),bEvaluation);Report->SetNumberField(TEXT("tier"),TrainingTier);Report->SetNumberField(TEXT("seed"),TrainingSeed);
    Report->SetNumberField(TEXT("episodes"),TrainingEpisodes);Report->SetNumberField(TEXT("wins"),TrainingWins);Report->SetNumberField(TEXT("losses"),TrainingLosses);Report->SetNumberField(TEXT("draws"),TrainingDraws);Report->SetNumberField(TEXT("updates"),Policy.Updates);
    Report->SetNumberField(TEXT("simulation_seconds"),TrainingClock);Report->SetNumberField(TEXT("wall_seconds"),FPlatformTime::Seconds()-TrainingWallStart);Report->SetNumberField(TEXT("fixed_step"),WarlockRules::Step);
    Report->SetStringField(TEXT("opponent"),TEXT("frozen scripted standard bot, mirrored legal shop curriculum, duel"));Report->SetStringField(TEXT("scope"),TEXT("skills/tactical selection only; scripted movement, 90s truncation, delayed meteors quantized to 0.03s in training"));
    FString Text;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(TrainingOutput/TEXT("report.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp,Display,TEXT("WARLOCK_RL complete %s"),*Text);bFinished=true;FPlatformMisc::RequestExit(false);
}
