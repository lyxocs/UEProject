#include "WarlockAudio.h"
#include "WarlockGame.h"
#include "WarlockUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundWaveProcedural.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool UWarlockAudio::DoesSupportWorldType(EWorldType::Type Type) const
{return Type==EWorldType::Game||Type==EWorldType::PIE;}
void UWarlockAudio::PlayCelebration(int Kind,int Streak)
{
    if(GetWorld()->GetNetMode()==NM_DedicatedServer)return;
    double Now=GetWorld()->GetRealTimeSeconds();int Key=1000+Kind;
    if(auto Previous=LastPlayed.Find(Key);Previous&&Now-*Previous<(Kind==2?1.2:.15))return;LastPlayed.Add(Key,Now);
    auto Settings=Cast<UWarlockUserSettings>(UGameUserSettings::GetGameUserSettings());float Volume=(Settings?Settings->EffectsVolume:.75f)*.38f;
    if(Volume<=0)return;
    const bool Victory=Kind==2;const int Rate=24000;const float Length=Victory?1.1f:.44f;
    TArray<int16> PCM;PCM.SetNumZeroed(FMath::CeilToInt(Length*Rate));
    for(int I=0;I<PCM.Num();++I)
    {
        float T=float(I)/Rate,Envelope=(1-FMath::Exp(-T*100))*FMath::Exp(-T*(Victory?4.f:11.f));
        float Fundamental=Victory?146.832f:293.665f*(1+.035f*FMath::Min(Streak,4));
        float Tone=FMath::Sin(2*PI*Fundamental*T)+.38f*FMath::Sin(2*PI*Fundamental*(Victory?1.5f:2.f)*T)+.22f*FMath::Sin(2*PI*Fundamental*(Victory?1.189207f:3.f)*T);
        float Low=.45f*FMath::Sin(2*PI*73.416f*T)*FMath::Exp(-T*13);
        PCM[I]=int16(FMath::Clamp((Tone*Envelope+Low)*.43f,-.95f,.95f)*32767);
    }
    auto Wave=NewObject<USoundWaveProcedural>(this);Wave->SetSampleRate(Rate);Wave->NumChannels=1;Wave->Duration=Length;Wave->bLooping=false;
    Wave->QueueAudio(reinterpret_cast<const uint8*>(PCM.GetData()),PCM.Num()*sizeof(int16));
    if(auto Component=UGameplayStatics::SpawnSound2D(this,Wave,Volume,1,0,Concurrency,false,true))
    {FTimerHandle Handle;GetWorld()->GetTimerManager().SetTimer(Handle,FTimerDelegate::CreateWeakLambda(Component,[Component](){Component->FadeOut(.04f,0);}),Length,false);}
}
void UWarlockAudio::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
    if(World.GetNetMode()==NM_DedicatedServer)return;
    if(FParse::Param(FCommandLine::Get(),TEXT("WarlockTrain")))return;
    if(auto Mode=World.GetAuthGameMode<AWarlockMode>();Mode&&Mode->bTraining)return;
    auto Load=[](const FString& Name){return LoadObject<USoundWave>(nullptr,*(TEXT("/Game/WarlockArena/Audio/")+Name+TEXT(".")+Name));};
    for(int I=0;I<8;++I){CastSounds.Add(Load(FString::Printf(TEXT("S_Cast%d"),I)));HitSounds.Add(Load(FString::Printf(TEXT("S_Hit%d"),I)));}
    Stone=Load(TEXT("S_Stone"));Ground=Load(TEXT("S_Ground"));
    Concurrency=NewObject<USoundConcurrency>(this);Concurrency->Concurrency.MaxCount=24;Concurrency->Concurrency.RetriggerTime=.025f;
    for(auto Pair:{TPair<const TCHAR*,TObjectPtr<UAudioComponent>*>(TEXT("S_AbyssTheme"),&Music),TPair<const TCHAR*,TObjectPtr<UAudioComponent>*>(TEXT("S_BattlePercussion"),&BattleLayer)})
    {
        if(auto Sound=Load(Pair.Key))
        {auto Component=UGameplayStatics::CreateSound2D(this,Sound,.001f,1,0,nullptr,false,false);*Pair.Value=Component;if(Component){Component->bIsUISound=true;Component->Play();}}
    }
}
void UWarlockAudio::Tick(float Dt)
{
    auto Settings=Cast<UWarlockUserSettings>(UGameUserSettings::GetGameUserSettings());
    const float Volume=Settings?FMath::Clamp(Settings->MusicVolume,0.f,1.f):.28f;
    auto State=GetWorld()->GetGameState<AWarlockMatchState>();const bool Fight=State&&State->Phase==TEXT("FIGHT");
    const float Duck=UGameplayStatics::IsGamePaused(this)?.35f:1.f;
    if(Music)Music->SetVolumeMultiplier(FMath::FInterpTo(Music->VolumeMultiplier,Volume*Duck,Dt,2.f));
    if(BattleLayer)BattleLayer->SetVolumeMultiplier(FMath::FInterpTo(BattleLayer->VolumeMultiplier,Volume*Duck*(Fight?.60f:.06f),Dt,1.8f));
}
void UWarlockAudio::Deinitialize()
{
    for(auto Component:{Music.Get(),BattleLayer.Get()})if(Component){Component->Stop();Component->DestroyComponent();}
    Music=BattleLayer=nullptr;Super::Deinitialize();
}
void UWarlockAudio::Event(UWorld* World,int Slot,int Surface,FVector Position)
{if(World)if(auto Audio=World->GetSubsystem<UWarlockAudio>())Audio->PlayEvent(Slot,Surface,Position);}
void UWarlockAudio::PlayEvent(int Slot,int Surface,FVector Position)
{
    if(GetWorld()->GetNetMode()==NM_DedicatedServer||Slot<-1||Slot>7||CastSounds.Num()!=8||HitSounds.Num()!=8)return;
    if(auto Mode=GetWorld()->GetAuthGameMode<AWarlockMode>();Mode&&Mode->bTraining)return;
    const int Key=Slot+1+Surface*9;double Now=GetWorld()->GetTimeSeconds();
    if(auto Previous=LastPlayed.Find(Key);Previous&&Now-*Previous<(Slot==7&&Surface==1?.25:.065))return;
    LastPlayed.Add(Key,Now);
    auto Settings=Cast<UWarlockUserSettings>(UGameUserSettings::GetGameUserSettings());
    float Volume=Settings?FMath::Clamp(Settings->EffectsVolume,0.f,1.f):.75f;
    // Both local players share a mix; nearest pawn drives audibility, not P1 only.
    float Distance=MAX_flt;
    for(auto It=GetWorld()->GetPlayerControllerIterator();It;++It)if(It->Get()->IsLocalController()&&It->Get()->GetPawn())Distance=FMath::Min(Distance,FVector::Dist2D(Position,It->Get()->GetPawn()->GetActorLocation()));
    if(Distance!=MAX_flt)Volume*=FMath::Lerp(1.f,.12f,FMath::Clamp(Distance/2000.f,0.f,1.f));
    USoundWave* Sound=Slot<0?nullptr:Surface==0?CastSounds[Slot].Get():HitSounds[Slot].Get();
    if(Sound)UGameplayStatics::SpawnSound2D(this,Sound,Volume*(Surface==0?.60f:.50f),FMath::FRandRange(.97f,1.03f),0,Concurrency,false,true);
    if(Surface>=2)if(auto Layer=Surface==2?Stone:Ground)UGameplayStatics::SpawnSound2D(this,Layer,Volume*.30f,1,0,Concurrency,false,true);
    ++PlayedEvents;LastSlot=Slot;LastSurface=Surface;
}
