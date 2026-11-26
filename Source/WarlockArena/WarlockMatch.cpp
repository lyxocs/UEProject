#include "WarlockGame.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GameMapsSettings.h"
#include "Engine/GameInstance.h"
#include "WarlockTuning.h"
#include "WarlockSpellFX.h"

AWarlockMode::AWarlockMode()
{
    PrimaryActorTick.bCanEverTick=true;
    DefaultPawnClass=AWarlockPawn::StaticClass(); PlayerControllerClass=AWarlockController::StaticClass();
    PlayerStateClass=AWarlockState::StaticClass(); GameStateClass=AWarlockMatchState::StaticClass(); HUDClass=AWarlockHUD::StaticClass();
}
AWarlockMatchState* AWarlockMode::Match() const {return GetGameState<AWarlockMatchState>();}
void AWarlockMode::BeginPlay()
{
    Super::BeginPlay(); Arena=GetWorld()->SpawnActor<AWarlockArena>();
    UGameMapsSettings::GetGameMapsSettings()->SetSkipAssigningGamepadToPlayer1(false);
    GetWorldTimerManager().SetTimerForNextTick(this,&AWarlockMode::EnsureRoster);
    if(UGameplayStatics::ParseOption(OptionsString,TEXT("LocalPlayers"))==TEXT("2"))
    {
        FString Input=UGameplayStatics::ParseOption(OptionsString,TEXT("Input"));
        bool Mixed=Input==TEXT("Mixed");
        UGameMapsSettings::GetGameMapsSettings()->SetSkipAssigningGamepadToPlayer1(Mixed);
        GetWorldTimerManager().SetTimerForNextTick([this,Input](){if(auto C=Cast<AWarlockController>(UGameplayStatics::GetPlayerController(this,0))){if(Input==TEXT("KeyboardMouse"))C->AddKeyboardMousePlayer();else if(Input==TEXT("Mixed"))C->AddMixedPlayer();else C->AddLocalPlayer();}});
    }
}
void AWarlockMode::PreLogin(const FString& Options,const FString& Address,const FUniqueNetIdRepl& UniqueId,FString& ErrorMessage)
{
    Super::PreLogin(Options,Address,UniqueId,ErrorMessage);
    if(GetNumPlayers()>=4)ErrorMessage=TEXT("The four-player room is full");
    else if(Match()&&Match()->Phase!=TEXT("SHOP"))ErrorMessage=TEXT("Join between rounds");
}
void AWarlockMode::PostLogin(APlayerController* P)
{
    Super::PostLogin(P);
    UE_LOG(LogTemp,Display,TEXT("Warlock: human joined; humans=%d"),GetNumPlayers());
    if(auto S=P->GetPlayerState<AWarlockState>()) S->SetPlayerName(FString::Printf(TEXT("术士 %d"),GetNumPlayers()));
    GetWorldTimerManager().SetTimerForNextTick(this,&AWarlockMode::EnsureRoster);
}
void AWarlockMode::Logout(AController* C)
{
    if(auto P=Cast<AWarlockPawn>(C->GetPawn())) {Fighters.Remove(P); P->Destroy();}
    Super::Logout(C);
}
void AWarlockMode::EnsureRoster()
{
    if(!Match())return;
    Fighters.RemoveAll([](const TObjectPtr<AWarlockPawn>& P){return !IsValid(P);});
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
    {
        auto C=It->Get();
        if(!C->GetPawn()) {auto P=GetWorld()->SpawnActor<AWarlockPawn>(); C->Possess(P); Fighters.AddUnique(P);}
    }
    while(Fighters.Num()>4)
    {
        auto Index=Fighters.IndexOfByPredicate([](const TObjectPtr<AWarlockPawn>& P){return P->State()&&P->State()->bBot;});
        if(Index==INDEX_NONE)break;
        auto P=Fighters[Index]; auto C=P->GetController(); Fighters.RemoveAt(Index); P->Destroy(); if(C)C->Destroy();
    }
    while(Fighters.Num()<4)
    {
        auto C=GetWorld()->SpawnActor<AWarlockAI>(); auto P=GetWorld()->SpawnActor<AWarlockPawn>(); C->Possess(P); Fighters.AddUnique(P);
        if(auto S=P->State()){S->bBot=true;S->Appearance=(Fighters.Num()-1)%3; S->SetPlayerName(FString::Printf(TEXT("余烬守卫 %d"),Fighters.Num()-1));}
    }
    for(int I=0;I<Fighters.Num();++I)
    {
        auto P=Fighters[I]; if(auto S=P->State()) S->Team=Match()->bTeams?(S->bBot?1:0):I;
        if(Match()->Phase==TEXT("SHOP")){P->ResetForRound(FVector(FMath::Cos(I*PI/2)*700,FMath::Sin(I*PI/2)*700,0));if(auto S=P->State()){S->bReady=S->bBot;S->ForceNetUpdate();}}
    }
    for(auto P:Fighters) if(auto C=Cast<AWarlockAI>(P->GetController())){C->ResetPerception();C->Shop();}
    UE_LOG(LogTemp,Display,TEXT("Warlock: roster=%d humans=%d"),Fighters.Num(),GetNumPlayers());
}
bool AWarlockMode::AreHumansReady()
{
    int Humans=0;
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
    {auto S=It->Get()->GetPlayerState<AWarlockState>();if(!S||S->bBot||!S->bReady)return false;++Humans;}
    return Humans>0&&Humans==GetNumPlayers();
}
void AWarlockMode::CheckReady(){if(Match()&&Match()->Phase==TEXT("SHOP")&&AreHumansReady())StartRound();}
void AWarlockMode::Tick(float Dt)
{
    Super::Tick(Dt); auto M=Match(); if(!M||bFinished)return;
    // Preserve elapsed simulation time. Do not silently discard hitches or accelerate timers.
    Accumulator+=Dt;
    while(Accumulator>=WarlockRules::Step)
    {
        const float Step=WarlockRules::Step; Accumulator-=Step;
        M->Remaining-=Step;
        if(M->Phase==TEXT("SHOP"))
        {
            if(M->Remaining<=0||AreHumansReady())StartRound();
            continue;
        }
        if(M->Phase==TEXT("RESULT"))
        {
            if(M->Remaining<=0)
            {
                // End after 11 rounds unless the top score is tied.
                int Best=MIN_int32,Count=0,WinningTeam=-1; TMap<int,int> Scores;
                for(auto P:Fighters)if(auto S=P->State())Scores.FindOrAdd(S->Team)+=S->Points;
                for(const auto& Pair:Scores){if(Pair.Value>Best){Best=Pair.Value;Count=1;WinningTeam=Pair.Key;}else if(Pair.Value==Best)++Count;}
                if(M->Round>=11&&Count==1){bFinished=true;M->Phase=TEXT("FINAL");
                    FString Winner=M->bTeams?(WinningTeam==0?TEXT("术士小队"):TEXT("余烬守卫")):TEXT("术士");
                    if(!M->bTeams)for(auto P:Fighters)if(P->State()&&P->State()->Team==WinningTeam)Winner=P->State()->GetPlayerName();
                    M->Announcement=FString::Printf(TEXT("%s 获胜 · %d 分"),*Winner,Best);}
                else {M->Phase=TEXT("SHOP");M->Remaining=30; M->Announcement=TEXT("准备下一回合的构筑");
                    for(auto P:Fighters){if(auto S=P->State())S->Gold+=10;} EnsureRoster();}
            }
            continue;
        }
        for(auto P:Fighters)if(IsValid(P))P->StepSimulation(Step);
        if(!bVisualPreview)for(auto P:Fighters)if(auto AI=Cast<AWarlockAI>(P->GetController()))AI->UpdateTactics(Step);
        SimulateProjectiles(Step);
        GroundAccumulator+=Step;
        while(GroundAccumulator>=WarlockRules::GroundStep)
        {
            GroundAccumulator-=WarlockRules::GroundStep;
            // Finish the entire ground batch before evaluating victory (BUG05).
            for(auto P:Fighters)if(P->bAlive)P->StepGround();
        }
        CheckVictory();
        if(M->Phase!=TEXT("FIGHT"))continue;
        ShrinkTimer-=Step;
        if(ShrinkTimer<=0)
        {
            M->ArenaRadius=FMath::Max(80.f,M->ArenaRadius-128.f);
            int Alive=0;for(auto P:Fighters)Alive+=P->bAlive;
            ShrinkTimer=15*FMath::Sqrt(float(FMath::Max(1,Alive)));
        }
        BotThinkTimer-=Step;
        if(BotThinkTimer<=0){BotThinkTimer+=.22f;if(!bVisualPreview)for(auto P:Fighters)if(auto C=Cast<AWarlockAI>(P->GetController()))C->Think();}
    }
}
void AWarlockMode::StartRound()
{
    if(!Match()||Match()->Phase!=TEXT("SHOP"))return;
    for(TActorIterator<AWarlockSpellFX> It(GetWorld());It;++It)It->Destroy();
    auto M=Match(); ++RoundGeneration; ++M->Round; M->Phase=TEXT("FIGHT"); M->Remaining=0; M->ArenaRadius=1280;
    UE_LOG(LogTemp,Display,TEXT("Warlock: round %d started"),M->Round);
    M->Announcement=TEXT("开场保护 · 4 秒"); GroundAccumulator=0; BotThinkTimer=0;
    for(TActorIterator<AWarlockProjectile> It(GetWorld());It;++It)It->Destroy();
    for(int I=0;I<Fighters.Num();++I){Fighters[I]->ResetForRound(FVector(FMath::Cos(I*PI/2)*700,FMath::Sin(I*PI/2)*700,0));if(auto AI=Cast<AWarlockAI>(Fighters[I]->GetController()))AI->ResetPerception();}
    ShrinkTimer=15*FMath::Sqrt(float(Fighters.Num()));
    if(Arena)Arena->PillarHealth.Init(40,4);
}
void AWarlockMode::CheckVictory()
{
    if(Match()->Phase!=TEXT("FIGHT"))return;
    TSet<int> AliveTeams; for(auto P:Fighters)if(P->bAlive&&P->State())AliveTeams.Add(P->State()->Team);
    if(AliveTeams.Num()<=1)EndRound(AliveTeams.Num()?*AliveTeams.CreateConstIterator():-1);
}
void AWarlockMode::EndRound(int Team)
{
    auto M=Match(); M->Phase=TEXT("RESULT");M->Remaining=4;
    M->Announcement=Team<0?TEXT("平局 · 无人生还"):TEXT("回合获胜");
    for(auto P:Fighters)if(P->bAlive&&P->State()&&P->State()->Team==Team){P->State()->Points++; M->Announcement=P->State()->GetPlayerName()+TEXT(" 存活获胜");}
    ++RoundGeneration;
    for(auto P:Fighters){P->CancelCast();P->MoveInput=FVector::ZeroVector;P->LocomotionSpeed=0;P->bClickMoving=false;P->ShotsLeft=0;P->ChainTime=0;P->ForceNetUpdate();}
    for(TActorIterator<AWarlockProjectile> It(GetWorld());It;++It)It->Destroy();
}
void AWarlockMode::RestartMatch(bool Teams)
{
    bVisualPreview=false;
    ++RoundGeneration; bFinished=false;Accumulator=GroundAccumulator=BotThinkTimer=ShrinkTimer=0; auto M=Match();M->bTeams=Teams;M->Round=0;M->Phase=TEXT("SHOP");M->Remaining=40;M->ArenaRadius=1280;
    for(auto P:Fighters)if(auto S=P->State())
    {S->Gold=20;S->Points=0;S->Levels.Init(0,8);S->Levels[0]=S->Levels[1]=1;S->Modes.Init(false,8);S->Items.Init(0,8);S->LifeMastery=S->RangeMastery=S->TimeMastery=0;S->bHasStone=false;S->StoneSlot=-1;}
    for(TActorIterator<AWarlockProjectile> It(GetWorld());It;++It)It->Destroy();
    for(TActorIterator<AWarlockPulse> It(GetWorld());It;++It)It->Destroy();
    for(TActorIterator<AWarlockSpellFX> It(GetWorld());It;++It)It->Destroy();
    EnsureRoster();
}
int AWarlockMode::Price(AWarlockPawn* P,int Product) const
{
    auto S=P?P->State():nullptr;if(!S)return -1;
    if(Product>=0&&Product<8)
    {
        if(Product==1)return -1;
        int L=S->Level(Product); int Cap=WarlockRules::LevelCap(Product)+(S->StoneSlot==Product?2:0);
        if(L>=Cap)return -1;
        const int Mastery=S->LifeMastery+S->RangeMastery+S->TimeMastery;
        if(Product==0&&Mastery<(L+1)/2)return -1; // R002 grqc: 1,1,2,2,3,3,...
        if(Product==2&&L>=3&&S->RangeMastery<(L==3||L==4?1:L==5?2:3))return -1;
        if(Product==5&&L>=4&&Mastery<1)return -1;
        if(Product==4&&L>=6&&S->RangeMastery<1)return -1;
        return L?WarlockRules::UpgradeCost(Product,L):WarlockRules::LearnCost(Product)+FMath::Max(0,S->LearnedCount()-2);
    }
    if(Product>=8&&Product<=10)
    {
        int L=Product==8?S->LifeMastery:Product==9?S->RangeMastery:S->TimeMastery;
        return L>=6?-1:(Product==8?6:Product==9?5:4)+L;
    }
    if(Product==11)return S->LifeMastery>=6||S->RangeMastery>=6||S->TimeMastery>=6?-1:13+S->LifeMastery+S->RangeMastery+S->TimeMastery;
    if(Product>=12&&Product<20)
    {
        int I=Product-12,L=S->Items[I];const int Prices[]={4,7,8,1,5,3,4,6},Caps[]={1,3,2,1,3,3,3,3};
        if(L>=Caps[I]||(I==1&&S->Items[4])||(I==4&&S->Items[1]))return -1;
        if(I==3)return -1; // Stone needs an explicit slot choice; not offered until that UI exists.
        if(L==0){int Used=0;for(int K=0;K<8;++K)Used+=S->Items[K]>0;if(Used>=2)return -1;}
        return Prices[I];
    }
    return -1;
}
bool AWarlockMode::Buy(AWarlockPawn* P,int Product,FString& Reason)
{
    if(!P||!P->State()||Match()->Phase!=TEXT("SHOP")){Reason=TEXT("只能在准备阶段购买");return false;}
    int Cost=Price(P,Product);auto S=P->State();
    if(Cost<0){Reason=TEXT("暂不可购买：检查等级上限、精通前置、两格容量或互斥装备");return false;}
    if(S->Gold<Cost){Reason=TEXT("金币不足");return false;}
    S->Gold-=Cost;S->bReady=S->bBot;
    if(Product<8)S->Levels[Product]++;
    else if(Product==8)S->LifeMastery++;
    else if(Product==9)S->RangeMastery++;
    else if(Product==10)S->TimeMastery++;
    else if(Product==11){S->LifeMastery++;S->RangeMastery++;S->TimeMastery++;}
    else S->Items[Product-12]++;
    P->Health=P->MaxHealth(); Reason=TEXT("购买成功");S->ForceNetUpdate();return true;
}
void AWarlockMode::Pulse(FVector Position,float Size,int Kind)
{
    AWarlockSpellFX::Spawn(GetWorld(),Kind==2?4:Kind==1?5:8,Position,Position,Size);
}
void AWarlockMode::Fire(AWarlockPawn* From,FVector Direction,float Damage,float Push,float Speed,float Life,int Kind)
{
    const auto Tuning=GetDefault<UWarlockTuning>();
    const float Scale=FMath::Clamp(Kind==0?Tuning->FireballSpeedScale:Tuning->ProjectileSpeedScale,.3f,1.5f);
    Speed*=Scale;Life/=Scale; // Preserve travel range while making projectiles readable.
    FVector D=Direction.GetSafeNormal2D();if(D.IsNearlyZero())return;
    FActorSpawnParameters Params;Params.bDeferConstruction=true;
    auto A=GetWorld()->SpawnActor<AWarlockProjectile>(AWarlockProjectile::StaticClass(),From->GetActorLocation(),D.Rotation(),Params);
    if(A){A->Caster=From;A->Velocity=D*Speed;A->Damage=Damage;A->Push=Push;A->Life=Life;A->Kind=Kind;A->Radius=Kind==1?22:Kind==2?35:25;A->FinishSpawning(FTransform(A->GetActorLocation()));}
}
void AWarlockMode::SimulateProjectiles(float Dt)
{
    struct FContact{float Time;int A;int B;AWarlockPawn* Pawn;int Pillar;};
    TArray<AWarlockProjectile*> Shots;TArray<FVector> Starts,Ends;TArray<FContact> Events;
    for(TActorIterator<AWarlockProjectile> It(GetWorld());It;++It)
    {
        auto P=*It;if(P->IsActorBeingDestroyed()||!P->Caster)continue;
        P->Life-=Dt;if(P->Life<=0){P->Destroy();continue;}
        Shots.Add(P);Starts.Add(P->GetActorLocation());Ends.Add(P->GetActorLocation()+P->Velocity*Dt);
    }
    for(int I=0;I<Shots.Num();++I)
    {
        auto Shot=Shots[I];float T;
        for(auto P:Fighters)if(P->bAlive&&P!=Shot->LastHit.Get()&&Shot->Caster->IsEnemy(P)&&WarlockRules::SweepCircle(Starts[I],Ends[I],P->GetActorLocation(),(P->ShieldRemaining>0?120.f:30.f)+Shot->Radius,T))Events.Add({T,I,-1,P,-1});
        if(Arena)for(int K=0;K<Arena->Pillars.Num();++K)if(Arena->PillarHealth[K]>0&&WarlockRules::SweepCircle(Starts[I],Ends[I],Arena->Pillars[K],35+Shot->Radius,T))Events.Add({T,I,-1,nullptr,K});
        for(int J=I+1;J<Shots.Num();++J)
        {
            if(Shot->Kind==2||Shots[J]->Kind==2||!Shot->Caster->IsEnemy(Shots[J]->Caster))continue;
            if(WarlockRules::SweepCircle(Starts[I]-Starts[J],Ends[I]-Ends[J],FVector::ZeroVector,Shot->Radius+Shots[J]->Radius,T))Events.Add({T,I,J,nullptr,-1});
        }
    }
    Events.StableSort([](const FContact& A,const FContact& B){return A.Time<B.Time;});TSet<int> Resolved;
    for(auto& Event:Events)
    {
        auto Shot=Shots[Event.A];if(Resolved.Contains(Event.A)||Shot->IsActorBeingDestroyed())continue;
        FVector Point=FMath::Lerp(Starts[Event.A],Ends[Event.A],Event.Time);
        if(Event.B>=0)
        {
            auto Other=Shots[Event.B];if(Resolved.Contains(Event.B)||Other->IsActorBeingDestroyed())continue;
            Shot->SetActorLocation(Point);Other->SetActorLocation(FMath::Lerp(Starts[Event.B],Ends[Event.B],Event.Time));
            Pulse((Point+Other->GetActorLocation())/2,40,0);Shot->Destroy();Other->Destroy();Resolved.Add(Event.A);Resolved.Add(Event.B);continue;
        }
        if(Event.Pawn)
        {
            auto Target=Event.Pawn;if(!Target->bAlive||!Shot->Caster->IsEnemy(Target))continue;
            if(Target->ShieldRemaining>0&&Shot->Bounces<8)
            {
                FVector N=(Point-Target->GetActorLocation()).GetSafeNormal2D();if(N.IsNearlyZero())N=-Shot->Velocity.GetSafeNormal2D();
                Shot->Velocity-=2*FVector::DotProduct(Shot->Velocity,N)*N;Shot->Caster=Target;Shot->LastHit=Target;++Shot->Bounces;
                Shot->SetActorLocation(Point+N*.1f+Shot->Velocity*Dt*(1-Event.Time));Shot->SetActorRotation(Shot->Velocity.Rotation());Shot->ForceNetUpdate();
            }
            else
            {
                if(Shot->Kind==2){Shot->Caster->ChainTarget=Target;Shot->Caster->ChainTime=MAX_flt;Shot->Caster->ChainSteps=0;Shot->Caster->ChainDamageTimer=.18f;}
                else Target->Hit(Shot->Caster,Shot->Damage,Shot->Velocity,Shot->Push);
                Shot->SetActorLocation(Point);Shot->Destroy();
            }
        }
        else if(Event.Pillar>=0)
        {
            if(!Arena||Arena->PillarHealth[Event.Pillar]<=0)continue;
            if(Shot->Kind==2){Shot->Caster->ChainTarget.Reset();Shot->Caster->ChainAnchor=Arena->Pillars[Event.Pillar]+FVector(0,0,40);Shot->Caster->ChainTime=MAX_flt;Shot->Caster->ChainSteps=0;}
            else Arena->DamagePillar(Event.Pillar,Shot->Damage,Shot->Caster);
            Shot->SetActorLocation(Point);Shot->Destroy();
        }
        Resolved.Add(Event.A);
    }
    for(int I=0;I<Shots.Num();++I)if(!Resolved.Contains(I)&&!Shots[I]->IsActorBeingDestroyed())Shots[I]->SetActorLocation(Ends[I]);
}
void AWarlockMode::Meteor(AWarlockPawn* From,FVector Target,int Level,bool Rolling)
{
    // Only the source map's first mode is enabled in this build.
    const int Generation=RoundGeneration;TWeakObjectPtr<AWarlockPawn> MeteorCaster=From;
    Pulse(Target,85*FMath::Sqrt(6.f+Level),2);
    FTimerHandle Timer;
    GetWorldTimerManager().SetTimer(Timer,[this,MeteorCaster,Target,Level,Generation]()
    {
        if(Generation!=RoundGeneration||Match()->Phase!=TEXT("FIGHT")||!MeteorCaster.IsValid())return;
        Pulse(Target,85*FMath::Sqrt(6.f+Level),0);
        for(auto P:Fighters)if(P->bAlive&&MeteorCaster->IsEnemy(P))
        {float D=FVector::Dist2D(Target,P->GetActorLocation());float Damage=10+Level-FMath::Square(D/85);if(Damage>4)P->Hit(MeteorCaster.Get(),Damage,P->GetActorLocation()-Target,.66f);}
    },1.15f,false);
}
