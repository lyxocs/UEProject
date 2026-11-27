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
    if(TrainingTier==6&&!bEvaluation)
    {
        auto Opp=CastChecked<AWarlockAI>(Fighters[1]->GetController());Opp->DifficultyOverride=TrainingEpisodes%3?6:5;Opp->bDisablePolicy=false;
        if(Opp->DifficultyOverride==6&&!TrainingLeague.IsEmpty())Opp->FrozenExpert=MakeShared<FWarlockExpertPolicy>(TrainingLeague[(TrainingSeed+TrainingEpisodes)%TrainingLeague.Num()]);else Opp->FrozenExpert.Reset();
    }
    for(int I=0;I<Fighters.Num();++I){auto P=Fighters[I];auto S=P->State();S->Team=I;S->bBot=true;S->Gold=20+10*(TrainingEpisodes%(TrainingTier==6?19:7));S->Points=0;
        S->Levels.Init(0,8);S->Levels[0]=S->Levels[1]=1;S->Modes.Init(false,8);S->Items.Init(0,8);S->LifeMastery=S->RangeMastery=S->TimeMastery=0;S->bHasStone=false;S->StoneSlot=-1;S->DefenseChoice=0;S->Families.Reset();for(int SlotIndex=0;SlotIndex<8;++SlotIndex)S->Families.Add(SlotIndex);
        auto AI=CastChecked<AWarlockAI>(P->GetController());AI->Experience.Action=-1;AI->ExpertExperience={};AI->Random.Initialize(TrainingSeed+TrainingEpisodes*101+I*7919);AI->ShopPlanOverride=TrainingEpisodes%(TrainingTier==6?6:3);AI->Shop();}
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
        FParse::Value(FCommandLine::Get(),TEXT("TrainSeed="),TrainingSeed);FParse::Value(FCommandLine::Get(),TEXT("TrainTier="),TrainingTier);TrainingTier=FMath::Clamp(TrainingTier,1,6);
        FParse::Value(FCommandLine::Get(),TEXT("TrainOpponentTier="),TrainingOpponentTier);TrainingOpponentTier=FMath::Clamp(TrainingOpponentTier,1,6);
        FParse::Value(FCommandLine::Get(),TEXT("TrainOutput="),TrainingOutput);if(TrainingOutput.IsEmpty())TrainingOutput=FPaths::ProjectSavedDir()/TEXT("Training/run");
        IFileManager::Get().MakeDirectory(*TrainingOutput,true);bEvaluation=FParse::Param(FCommandLine::Get(),TEXT("TrainEvaluate"));
        auto& Policy=FWarlockPolicy::Shared();auto& Expert=FWarlockExpertPolicy::Shared();FString Initial;if(FParse::Value(FCommandLine::Get(),TEXT("TrainPolicy="),Initial)&&!(TrainingTier==6?Expert.Load(Initial):Policy.Load(Initial))){UE_LOG(LogTemp,Error,TEXT("Training policy could not be loaded"));FPlatformMisc::RequestExitWithStatus(false,2);return;}
        auto Player=GetWorld()->GetFirstPlayerController();auto Pawn=CastChecked<AWarlockPawn>(Player->GetPawn());Player->UnPossess();auto Learner=GetWorld()->SpawnActor<AWarlockAI>();Learner->Possess(Pawn);
        for(int I=Fighters.Num()-1;I>=2;--I){auto P=Fighters[I];auto C=P->GetController();P->Destroy();C->Destroy();Fighters.RemoveAt(I);}
        Learner->DifficultyOverride=TrainingTier;Learner->bLearning=!bEvaluation;Learner->bDisablePolicy=FParse::Param(FCommandLine::Get(),TEXT("TrainBaseline"));
        if(FParse::Param(FCommandLine::Get(),TEXT("TrainUntrained"))){if(TrainingTier==6){Expert=FWarlockExpertPolicy();Expert.bReady=true;}else {Policy=FWarlockPolicy();Policy.bReady=true;}}
        if(TrainingTier==6){Expert.Save(TrainingOutput/TEXT("initial-policy.json"));TrainingLeague.Add(Expert);}else Policy.Save(TrainingOutput/TEXT("initial-policy.json"));
        auto Opponent=CastChecked<AWarlockAI>(Fighters[1]->GetController());Opponent->bDisablePolicy=TrainingTier!=6;Opponent->DifficultyOverride=TrainingTier==6?TrainingOpponentTier:3;
        FString Frozen;if(FParse::Value(FCommandLine::Get(),TEXT("TrainOpponentPolicy="),Frozen)){Opponent->FrozenExpert=MakeShared<FWarlockExpertPolicy>();if(!Opponent->FrozenExpert->Load(Frozen)){FPlatformMisc::RequestExitWithStatus(false,2);return;}Opponent->DifficultyOverride=6;Opponent->bDisablePolicy=false;}
        ResetTrainingEpisode();UE_LOG(LogTemp,Display,TEXT("WARLOCK_RL started episodes=%d tier=%d eval=%d"),TrainingLimit,TrainingTier,bEvaluation);
    }
    // Fixed-size batches yield to UE between batches for actor cleanup and GC.
    bTrainingStep=true;
    for(int StepIndex=0;StepIndex<128;++StepIndex)
    {
        TrainingClock+=WarlockRules::Step;TrainingEpisodeTime+=WarlockRules::Step;Tick(WarlockRules::Step);
        if(Match()->Phase==TEXT("FIGHT")&&TrainingEpisodeTime<90)continue;
        auto Learner=CastChecked<AWarlockAI>(Fighters[0]->GetController());bool Win=Fighters[0]->bAlive&&!Fighters[1]->bAlive,Loss=!Fighters[0]->bAlive&&Fighters[1]->bAlive;
        if(TrainingTier==6)Learner->LearnExpert(true,Win?2.f:Loss?-2.f:0.f);else Learner->LearnTransition(true,Win?2.f:Loss?-2.f:0.f);TrainingWins+=Win;TrainingLosses+=Loss;TrainingDraws+=!Win&&!Loss;++TrainingEpisodes;
        if(TrainingEpisodes%50==0){UE_LOG(LogTemp,Display,TEXT("WARLOCK_RL episodes=%d wins=%d losses=%d draws=%d updates=%d steps_per_second=%.0f"),TrainingEpisodes,TrainingWins,TrainingLosses,TrainingDraws,TrainingTier==6?FWarlockExpertPolicy::Shared().Updates:FWarlockPolicy::Shared().Updates,TrainingClock/WarlockRules::Step/(FPlatformTime::Seconds()-TrainingWallStart));
            if(!bEvaluation){if(TrainingTier==6){FWarlockExpertPolicy::Shared().Save(TrainingOutput/FString::Printf(TEXT("policy-%05d.json"),TrainingEpisodes));if(TrainingEpisodes%200==0){TrainingLeague.Add(FWarlockExpertPolicy::Shared());if(TrainingLeague.Num()>5)TrainingLeague.RemoveAt(0);}}else FWarlockPolicy::Shared().Save(TrainingOutput/FString::Printf(TEXT("policy-%05d.json"),TrainingEpisodes));}GEngine->ForceGarbageCollection(true);}
        float Budget=240;FParse::Value(FCommandLine::Get(),TEXT("TrainSeconds="),Budget);
        if(TrainingEpisodes>=TrainingLimit||(!bEvaluation&&FPlatformTime::Seconds()-TrainingWallStart>Budget)){FinishTraining();bTrainingStep=false;return;}
        ResetTrainingEpisode();
    }
    bTrainingStep=false;
}
void AWarlockMode::FinishTraining()
{
    auto& Policy=FWarlockPolicy::Shared();auto& Expert=FWarlockExpertPolicy::Shared();if(!bEvaluation){if(TrainingTier==6)Expert.Save(TrainingOutput/TEXT("policy.json"));else Policy.Save(TrainingOutput/TEXT("policy.json"));}
    auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("algorithm"),TEXT("masked softmax actor-critic TD(0)"));Report->SetBoolField(TEXT("evaluation"),bEvaluation);Report->SetNumberField(TEXT("tier"),TrainingTier);Report->SetNumberField(TEXT("seed"),TrainingSeed);
    Report->SetNumberField(TEXT("episodes"),TrainingEpisodes);Report->SetNumberField(TEXT("wins"),TrainingWins);Report->SetNumberField(TEXT("losses"),TrainingLosses);Report->SetNumberField(TEXT("draws"),TrainingDraws);Report->SetNumberField(TEXT("updates"),TrainingTier==6?Expert.Updates:Policy.Updates);
    Report->SetNumberField(TEXT("simulation_seconds"),TrainingClock);Report->SetNumberField(TEXT("wall_seconds"),FPlatformTime::Seconds()-TrainingWallStart);Report->SetNumberField(TEXT("fixed_step"),WarlockRules::Step);
    Report->SetStringField(TEXT("opponent"),TEXT("frozen scripted standard bot, mirrored legal shop curriculum, duel"));Report->SetStringField(TEXT("scope"),TEXT("skills/tactical selection only; scripted movement, 90s truncation, delayed meteors quantized to 0.03s in training"));
    if(TrainingTier==6){Report->SetStringField(TEXT("algorithm"),TEXT("multi-head masked actor-critic with frozen snapshot league"));Report->SetStringField(TEXT("opponent"),bEvaluation?FString::Printf(TEXT("fixed tier %d or explicit frozen policy"),TrainingOpponentTier):TEXT("tier5 and frozen tier6 snapshots"));Report->SetStringField(TEXT("scope"),TEXT("delayed sensor >=180ms; learned combat/movement/aim/shop with safety/legal masks; duel 90s cutoff"));Report->SetNumberField(TEXT("purchases"),TrainingPurchases);Report->SetNumberField(TEXT("feints_started"),TrainingFeints);Report->SetNumberField(TEXT("threat_avoidance_decisions"),TrainingDodges);TArray<TSharedPtr<FJsonValue>> Counts;for(int V:TrainingFamilyCasts)Counts.Add(MakeShared<FJsonValueNumber>(V));Report->SetArrayField(TEXT("family_cast_attempts"),Counts);}
    FString Text;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(TrainingOutput/TEXT("report.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp,Display,TEXT("WARLOCK_RL complete %s"),*Text);bFinished=true;FPlatformMisc::RequestExit(false);
}
