#include "WarlockLearning.h"
#include "WarlockGame.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FWarlockDifficulty FWarlockDifficulty::Get(int Tier)
{
    const FWarlockDifficulty Profiles[]={{1.05f,1.5f,1.25f,180,.12f,.55f},{.75f,1.1f,.9f,110,.35f,.3f},{.45f,.75f,.5f,45,.65f,.08f},{.36f,.6f,.4f,25,.8f,.02f},{.3f,.5f,.32f,12,.9f,0}};
    if(Tier==6)return {.18f,.24f,.18f,6,1,0};
    return Profiles[FMath::Clamp(Tier,1,5)-1];
}
const TCHAR* FWarlockDifficulty::Name(int Tier)
{const TCHAR* Names[]={TEXT("入门"),TEXT("简单"),TEXT("普通"),TEXT("困难"),TEXT("大师"),TEXT("宗师")};return Names[FMath::Clamp(Tier,1,6)-1];}
FWarlockPolicy& FWarlockPolicy::Shared()
{
    static FWarlockPolicy Policy;static bool Loaded=false;
    if(!Loaded){Loaded=true;Policy.Load(FPaths::ProjectContentDir()/TEXT("WarlockArena/AI/TacticalPolicy.json"));}
    return Policy;
}
bool FWarlockPolicy::Load(const FString& Path)
{
    FString Text;TSharedPtr<FJsonObject> Object;
    if(!FFileHelper::LoadFileToString(Text,*Path)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Object)||!Object.IsValid())return false;
    const TArray<TSharedPtr<FJsonValue>>* A;const TArray<TSharedPtr<FJsonValue>>* C;
    if(Object->GetIntegerField(TEXT("schema"))!=1||!Object->TryGetArrayField(TEXT("actor"),A)||A->Num()!=Features*Actions||!Object->TryGetArrayField(TEXT("critic"),C)||C->Num()!=Features)return false;
    FWarlockPolicy Parsed;
    for(int I=0;I<Features*Actions;++I){double V;if(!(*A)[I]->TryGetNumber(V)||!FMath::IsFinite(V)||FMath::Abs(V)>12)return false;Parsed.Actor[I/Features][I%Features]=V;}
    for(int I=0;I<Features;++I){double V;if(!(*C)[I]->TryGetNumber(V)||!FMath::IsFinite(V)||FMath::Abs(V)>20)return false;Parsed.Critic[I]=V;}
    Parsed.Updates=Object->GetIntegerField(TEXT("updates"));Parsed.bReady=Parsed.Updates>0;
    if(!Parsed.bReady)return false;*this=Parsed;return true;
}
bool FWarlockPolicy::Save(const FString& Path) const
{
    auto Object=MakeShared<FJsonObject>();Object->SetNumberField(TEXT("schema"),1);Object->SetNumberField(TEXT("updates"),Updates);
    Object->SetStringField(TEXT("algorithm"),TEXT("masked softmax actor-critic, TD(0), native authoritative rollouts"));
    TArray<TSharedPtr<FJsonValue>> A,C;for(auto& Row:Actor)for(float V:Row)A.Add(MakeShared<FJsonValueNumber>(V));for(float V:Critic)C.Add(MakeShared<FJsonValueNumber>(V));
    Object->SetArrayField(TEXT("actor"),A);Object->SetArrayField(TEXT("critic"),C);FString Text;FJsonSerializer::Serialize(Object,TJsonWriterFactory<>::Create(&Text));
    return FFileHelper::SaveStringToFile(Text,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
namespace
{
void Observe(AWarlockAI* AI,float* X)
{
    auto P=Cast<AWarlockPawn>(AI->GetPawn());auto T=AI->ObservedTarget.Get();auto M=P->GetWorld()->GetAuthGameMode<AWarlockMode>();
    float Distance=FVector::Dist2D(P->GetActorLocation(),AI->ObservedTargetPosition),Radius=FMath::Max(80.f,M->Match()->ArenaRadius);
    X[0]=1;X[1]=Distance/1000;X[2]=P->GetActorLocation().Size2D()/Radius;X[3]=P->Health/P->MaxHealth();X[4]=P->Knockback/100;
    X[5]=T?T->Health/T->MaxHealth():0;X[6]=T?T->Knockback/100:0;X[7]=AI->ObservedVelocity.Size2D()/600;X[8]=P->Momentum.Size2D()/20;
    X[9]=AI->ObservedTargetPosition.Size2D()/Radius;X[10]=AI->bObservedThreat?1:0;X[11]=Distance<250?1:0;
    for(int I=0;I<8;++I)X[12+I]=P->State()->Level(I)>0&&P->Cooldowns[I]<=0?1:0;
    for(int I=0;I<FWarlockPolicy::Features;++I)X[I]=FMath::Clamp(X[I],-2.f,2.f);
}
}
float FWarlockPolicy::Value(const float* X) const{float V=0;for(int J=0;J<Features;++J)V+=Critic[J]*X[J];return V;}
int FWarlockPolicy::Choose(AWarlockAI* AI,float* X,float* Probabilities)
{
    Observe(AI,X);auto P=Cast<AWarlockPawn>(AI->GetPawn());auto M=P->GetWorld()->GetAuthGameMode<AWarlockMode>();float D=FVector::Dist2D(P->GetActorLocation(),AI->ObservedTargetPosition);
    bool Edge=P->GetActorLocation().Size2D()>M->Match()->ArenaRadius-150;
    bool Mask[Actions]={D<1000,D<245,D<600,Edge,D<800,D<800,P->Knockback>25||AI->bObservedThreat,D>350&&D<800,false,false,true};
    for(int I=0;I<8;++I)Mask[I]&=X[12+I]>0;
    int Fake=D<360?1:5;Mask[8]=!Edge&&D>=180&&D<=750&&AI->FeintCooldown<=0&&X[12+Fake]>0;
    FVector Landing=AI->ObservedTargetPosition-(AI->ObservedTargetPosition-P->GetActorLocation()).GetSafeNormal2D()*135;
    Mask[9]=!Edge&&D>=330&&D<=700&&AI->ComboCooldown<=0&&X[15]>0&&X[13]>0&&Landing.Size2D()<M->Match()->ArenaRadius-110;
    if(AI->Difficulty().PolicyChance<.3f)Mask[8]=Mask[9]=false;
    float Max=-100,Logits[Actions],Sum=0;
    int Tier=AI->DifficultyOverride?AI->DifficultyOverride:M->Match()->AIDifficulty;
    // Training explores at temperature 1; high tiers exploit learned values.
    float Temperature=AI->bLearning?1.f:Tier==5?.22f:Tier==4?.55f:1.f;
    for(int I=0;I<Actions;++I){float L=I==10?-1.5f:I==2?.6f:0;for(int J=0;J<Features;++J)L+=Actor[I][J]*X[J];Logits[I]=FMath::Clamp(L/Temperature,-60.f,60.f);if(Mask[I])Max=FMath::Max(Max,Logits[I]);}
    for(int I=0;I<Actions;++I){Probabilities[I]=Mask[I]?FMath::Exp(Logits[I]-Max):0;Sum+=Probabilities[I];}
    float Draw=AI->Random.FRand();for(int I=0;I<Actions;++I){Probabilities[I]/=Sum;}
    for(int I=0;I<Actions;++I){Draw-=Probabilities[I];if(Draw<=0)return I;}return 10;
}
float AWarlockAI::DecisionTime() const{auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();return M&&M->bTraining?M->TrainingClock:GetWorld()->GetTimeSeconds();}
FWarlockDifficulty AWarlockAI::Difficulty() const{auto M=GetWorld()->GetGameState<AWarlockMatchState>();return FWarlockDifficulty::Get(DifficultyOverride?DifficultyOverride:M?M->AIDifficulty:3);}
float AWarlockAI::LearningScore() const
{
    auto P=Cast<AWarlockPawn>(GetPawn());if(!P)return 0;auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();
    float Enemy=0;int Count=0;for(auto T:M->Fighters)if(T!=P&&P->IsEnemy(T)){Enemy+=T->Health/T->MaxHealth();++Count;}
    return P->Health/P->MaxHealth()-Enemy/FMath::Max(1,Count);
}
void AWarlockAI::LearnTransition(bool Terminal,float Outcome)
{
    if(!bLearning||Experience.Action<0)return;auto& Policy=FWarlockPolicy::Shared();
    float X[FWarlockPolicy::Features]={};if(!Terminal)Observe(this,X);
    float Reward=LearningScore()-Experience.PreviousScore-.002f+Outcome;
    float Advantage=FMath::Clamp(Reward+(Terminal?0:.98f*Policy.Value(X))-Policy.Value(Experience.X),-2.f,2.f);
    for(int J=0;J<FWarlockPolicy::Features;++J){Policy.Critic[J]=FMath::Clamp(Policy.Critic[J]+.012f*Advantage*Experience.X[J],-10.f,10.f);
        for(int I=0;I<FWarlockPolicy::Actions;++I)Policy.Actor[I][J]=FMath::Clamp(Policy.Actor[I][J]+.004f*Advantage*((Experience.Action==I?1.f:0.f)-Experience.Probabilities[I])*Experience.X[J],-4.f,4.f);}
    ++Policy.Updates;Experience.Action=-1;
}
bool AWarlockAI::ExecutePolicyAction(int Action)
{
    auto P=Cast<AWarlockPawn>(GetPawn());if(Action==10)return true;
    if(Action==8)return TryFeint();if(Action==9)return TryBlinkSmite();
    if(Action==3)P->AimPoint=P->GetActorLocation()-P->GetActorLocation().GetSafeNormal2D()*700;
    return P->Cast(Action);
}
bool AWarlockAI::ChoosePolicyAction()
{
    auto& Policy=FWarlockPolicy::Shared();if(bDisablePolicy||(!Policy.bReady&&!bLearning))return false;
    LearnTransition();int Action=Policy.Choose(this,Experience.X,Experience.Probabilities);++PolicyDecisions;LastPolicyAction=Action;
    if(bLearning){Experience.Action=Action;Experience.PreviousScore=LearningScore();}
    ExecutePolicyAction(Action);NextDecision=DecisionTime()+Difficulty().Interval;return true;
}
