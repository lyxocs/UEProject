#include "WarlockExpert.h"
#include "WarlockGame.h"
#include "WarlockCatalog.h"
#include "WarlockSpellFX.h"
#include "WarlockTuning.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

FWarlockExpertPolicy& FWarlockExpertPolicy::Shared()
{static FWarlockExpertPolicy P;static bool Loaded=false;if(!Loaded){Loaded=true;P.Load(FPaths::ProjectContentDir()/TEXT("WarlockArena/AI/ExpertPolicy.json"));}return P;}
bool FWarlockExpertPolicy::Load(const FString& Path)
{
    FString Text;TSharedPtr<FJsonObject> O;const TArray<TSharedPtr<FJsonValue>> *A,*C;
    if(!FFileHelper::LoadFileToString(Text,*Path)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O)||!O.IsValid())return false;
    if(O->GetIntegerField(TEXT("schema"))!=2||!O->TryGetArrayField(TEXT("actor"),A)||A->Num()!=Features*Actions||!O->TryGetArrayField(TEXT("critic"),C)||C->Num()!=Features)return false;
    FWarlockExpertPolicy P;
    for(int I=0;I<A->Num();++I){double V;if(!(*A)[I]->TryGetNumber(V)||!FMath::IsFinite(V)||FMath::Abs(V)>8)return false;P.Actor[I/Features][I%Features]=V;}
    for(int I=0;I<Features;++I){double V;if(!(*C)[I]->TryGetNumber(V)||!FMath::IsFinite(V)||FMath::Abs(V)>20)return false;P.Critic[I]=V;}
    P.Updates=O->GetIntegerField(TEXT("updates"));P.bReady=true;*this=P;return true;
}
bool FWarlockExpertPolicy::Save(const FString& Path) const
{
    auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("schema"),2);O->SetNumberField(TEXT("updates"),Updates);O->SetNumberField(TEXT("features"),Features);O->SetNumberField(TEXT("actions"),Actions);
    O->SetStringField(TEXT("algorithm"),TEXT("masked multi-head actor-critic; delayed observations; combat/movement/aim/shop"));
    TArray<TSharedPtr<FJsonValue>> A,C;for(auto& Row:Actor)for(float V:Row)A.Add(MakeShared<FJsonValueNumber>(V));for(float V:Critic)C.Add(MakeShared<FJsonValueNumber>(V));O->SetArrayField(TEXT("actor"),A);O->SetArrayField(TEXT("critic"),C);
    FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));return FFileHelper::SaveStringToFile(Text,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
int FWarlockExpertPolicy::Choose(int First,int Count,const float* X,const bool* Mask,const float* Prior,FRandomStream& R,bool Explore,float* Prob) const
{
    float Logits[Actions]={},Max=-1e20f,Sum=0;int Best=First;
    for(int I=First;I<First+Count;++I)if(Mask[I]){float L=Prior[I];for(int J=0;J<Features;++J)L+=Actor[I][J]*X[J];Logits[I]=FMath::Clamp(L/(Explore?1.f:.35f),-60.f,60.f);if(Logits[I]>Max){Max=Logits[I];Best=I;}}
    for(int I=First;I<First+Count;++I){Prob[I]=Mask[I]?FMath::Exp(Logits[I]-Max):0;Sum+=Prob[I];}
    if(Sum<=0)return Best;
    for(int I=First;I<First+Count;++I)Prob[I]/=Sum;
    if(!Explore)return Best;
    float Draw=R.FRand();for(int I=First;I<First+Count;++I){Draw-=Prob[I];if(Draw<=0)return I;}return Best;
}
float FWarlockExpertPolicy::Value(const float* X) const{float V=0;for(int J=0;J<Features;++J)V+=Critic[J]*X[J];return V;}
void FWarlockExpertPolicy::Update(const float* X,int Action,const float* Prob,float Advantage,bool Shop)
{
    int First=Shop?11:Action<15?0:Action<24?15:24,End=Shop?61:Action<15?15:Action<24?24:27;
    for(int I=First;I<End;++I)for(int J=0;J<Features;++J)Actor[I][J]=FMath::Clamp(Actor[I][J]+.0015f*Advantage*((I==Action?1.f:0.f)-Prob[I])*X[J],-2.f,2.f);
    ++Updates;bReady=true;
}
FWarlockExpertPolicy& AWarlockAI::ExpertPolicy(){return FrozenExpert.IsValid()?*FrozenExpert:FWarlockExpertPolicy::Shared();}
void AWarlockAI::ExpertFeatures(float* X) const
{
    auto P=Cast<AWarlockPawn>(GetPawn());auto S=P->State();bool Shop=GetWorld()->GetGameState<AWarlockMatchState>()->Phase==TEXT("SHOP");
    FVector O=Shop?P->GetActorLocation():ExpertView.SelfPosition+ExpertView.SelfVelocity*FMath::Min(.35f,ExpertClock-ExpertView.Captured),D=ExpertView.Position-O;float Radius=FMath::Max(80.f,ExpertView.Radius);
    X[0]=1;X[1]=D.Size2D()/1000;X[2]=O.Size2D()/Radius;X[3]=Shop?P->Health/P->MaxHealth():ExpertView.SelfHealth/FMath::Max(1.f,ExpertView.SelfMaxHealth);X[4]=ExpertView.SelfKnockback/100;X[5]=ExpertView.Health;X[6]=ExpertView.Knockback/100;
    X[7]=ExpertView.Velocity.X/600;X[8]=ExpertView.Velocity.Y/600;X[9]=ExpertView.Position.Size2D()/Radius;X[10]=bObservedThreat;X[11]=ExpertView.Pending>=0;
    for(int I=0;I<8;++I){X[12+I]=S->Level(I)>0&&P->Cooldowns[I]<=0;X[20+I]=WarlockCatalog::Family(S,I)/22.f;X[28+I]=S->Level(I)/8.f;X[36+I]=S->Modes[I]?1.f:0.f;X[44+I]=P->Cooldowns[I]/30.f;}
    X[52]=S->Gold/100.f;X[53]=S->LearnedCount()/6.f;X[54]=S->LifeMastery/6.f;X[55]=S->RangeMastery/6.f;X[56]=S->TimeMastery/6.f;
    for(int I=0;I<6;++I)X[57+I]=S->Items[I]/3.f;X[63]=GetWorld()->GetGameState<AWarlockMatchState>()->Phase==TEXT("SHOP")?1:0;
    for(int I=0;I<64;++I)X[I]=FMath::Clamp(X[I],-2.f,2.f);
}
void AWarlockAI::LearnExpert(bool Terminal,float Outcome)
{
    if(!bLearning)return;auto& Model=ExpertPolicy();float X[64]={};ExpertFeatures(X);
    if(ExpertExperience.Action[0]>=0)
    {
        float Reward=LearningScore()-ExpertExperience.Score-.0005f+Outcome;
        float Advantage=FMath::Clamp(Reward+(Terminal?0:.985f*Model.Value(X))-Model.Value(ExpertExperience.X),-2.f,2.f);
        for(int J=0;J<64;++J)Model.Critic[J]=FMath::Clamp(Model.Critic[J]+.005f*Advantage*ExpertExperience.X[J],-10.f,10.f);
        for(int H=0;H<3;++H)if(ExpertExperience.Action[H]>=0)Model.Update(ExpertExperience.X,ExpertExperience.Action[H],ExpertExperience.Prob,Advantage);
    }
    if(Terminal){for(auto& E:ExpertPurchases)Model.Update(E.X,E.Action[3],E.Prob,FMath::Clamp(Outcome+LearningScore(),-2.f,2.f)/FMath::Sqrt(float(FMath::Max(1,ExpertPurchases.Num()))),true);ExpertPurchases.Reset();}
    ExpertExperience={};
}
namespace
{
int ShopProduct(int Action){int I=Action-27;return I<21?WarlockCatalog::Product(I+2):I<25?8+I-21:12+I-25;}
float Reach(int F,bool Alt,float R)
{
    using namespace WarlockCatalog;
    switch(F){case Lightning:return 600*R;case Dash:return 900*R;case WarlockCatalog::Swap:return 800*R;case Drain:return (Alt?850:700)*R;case Bounce:return 900*R;case Split:return 700*R;case Blast:return 500*R;case Meteor:return Alt?572*R:1100;case Bind:return 1100*R;case Control:return 1800*R;case Chain:return 817*R;default:return 1000*R;}
}
}
void AWarlockAI::ShopExpert()
{
    auto P=Cast<AWarlockPawn>(GetPawn());auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();auto S=P->State();ExpertPurchases.Reset();
    if(M->Match()->Phase!=TEXT("SHOP"))return;
    bool ModeChosen[4]={};
    // Different per-seat priors, learned choices and legal prices; no free items.
    static const int Routes[][6]={{2,3,4,5,6,7},{9,11,13,16,18,21},{10,12,14,17,19,20},{2,3,13,15,8,22},{9,12,14,5,19,21},{10,11,4,16,18,20}};
    for(int N=0;N<40;++N)
    {
        FWarlockExpertExperience E;ExpertFeatures(E.X);bool Mask[61]={};float Prior[61]={};int Legal=0;
        for(int I=27;I<60;++I)
        {
            int Product=ShopProduct(I),Cost=M->Price(P,Product);Mask[I]=Cost>0&&Cost<=S->Gold;if(!Mask[I])continue;++Legal;
            if(I<48){int F=I-27+2,Slot=WarlockCatalog::Slot(F);Prior[I]=S->Level(Slot)==0?1.6f:.1f;Prior[I]+=Routes[FMath::Abs(S->ColorIndex+ShopPlanOverride+1)%6][Slot-2]==F?.7f:0;}
            else if(I<52)Prior[I]=S->LearnedCount()>=2?.8f:-.8f;
            else {int Item=Product-12;Prior[I]=(Item==4&&S->Items[4]==0)?2.1f:(Item==1||Item==6||Item==7)&&S->Items[Item]==0?1.1f:.1f;}
        }
        for(int I=11;I<15;++I){int Slot=I==11?3:I==12?4:I==13?5:7;Mask[I]=!ModeChosen[I-11]&&S->Level(Slot)>0&&WarlockCatalog::Dual(WarlockCatalog::Family(S,Slot));Prior[I]=-.5f;}
        Mask[60]=true;Prior[60]=Legal?-2.f:2.f;
        FWarlockExpertPolicy Zero;auto& Model=bDisablePolicy?Zero:ExpertPolicy();int A=Model.Choose(11,50,E.X,Mask,Prior,Random,bLearning,E.Prob);if(A==60)break;
        FString Reason;if(A<15){int Slot=A==11?3:A==12?4:A==13?5:7;M->SetSkillMode(P,WarlockCatalog::Product(WarlockCatalog::Family(S,Slot)),Reason);ModeChosen[A-11]=true;if(bLearning){E.Action[3]=A;ExpertPurchases.Add(E);}continue;}
        int Product=ShopProduct(A);if(!M->Buy(P,Product,Reason))break;
        if(Product>=12&&Product<20)++ExpertItemPurchases;
        if(M->bTraining)++M->TrainingPurchases;
        if(bLearning){E.Action[3]=A;ExpertPurchases.Add(E);}
    }
}
void AWarlockAI::ThinkExpert(float Dt)
{
    auto P=Cast<AWarlockPawn>(GetPawn());auto M=GetWorld()->GetAuthGameMode<AWarlockMode>();if(!P||!P->bAlive||!M||M->Match()->Phase!=TEXT("FIGHT"))return;
    ExpertClock+=Dt;auto O=P->GetActorLocation();
    if(ExpertClock+UE_SMALL_NUMBER>=ExpertNextCapture)
    {
        ExpertNextCapture=ExpertClock+.06f;FWarlockExpertFrame F;F.Captured=ExpertClock;F.Ready=ExpertClock+Random.FRandRange(.18f,.24f);F.Radius=M->Match()->ArenaRadius;
        F.SelfPosition=O;F.SelfHealth=P->Health;F.SelfMaxHealth=P->MaxHealth();F.SelfKnockback=P->Knockback;F.SelfMomentum=P->Momentum;
        if(!ExpertFrames.IsEmpty())F.SelfVelocity=((O-ExpertFrames.Last().SelfPosition)/FMath::Max(.03f,ExpertClock-ExpertFrames.Last().Captured)).GetClampedToMaxSize2D(1500);
        float Best=MAX_flt;for(auto Other:M->Fighters)if(Other->bAlive&&P->IsEnemy(Other)&&(Other->StealthRemaining<=0||FVector::Dist2D(O,Other->GetActorLocation())<=100))
        {float Score=FVector::DistSquared2D(O,Other->GetActorLocation());if(Score<Best){Best=Score;F.Target=Other;}}
        if(auto T=F.Target.Get())
        {
            F.Position=T->GetActorLocation();F.Health=T->Health/T->MaxHealth();F.Knockback=T->Knockback;F.Pending=T->PendingCast;
            if(T->PendingCast==1)F.Threats.Add({F.Position,FVector::ZeroVector,255,T->CastRemaining,true});
            if(ExpertLastTarget==T&&ExpertLastCapture>0&&FVector::Dist2D(F.Position,ExpertLastPosition)<180)F.Velocity=((F.Position-ExpertLastPosition)/FMath::Max(.03f,ExpertClock-ExpertLastCapture)).GetClampedToMaxSize2D(600);
            ExpertLastTarget=T;ExpertLastPosition=F.Position;ExpertLastCapture=ExpertClock;
        }
        for(TActorIterator<AWarlockProjectile> It(GetWorld());It;++It)if(It->Caster&&It->Caster->IsEnemy(P)&&FVector::Dist2D(O,It->GetActorLocation())<1800)
            F.Threats.Add({It->GetActorLocation(),It->Velocity,It->Radius,It->Life,It->bField});
        // Only public meteor warnings. Training suppresses visuals, so use the
        // matching public impact queue there, never hidden opponent inputs.
        if(M->bTraining){for(auto& Job:M->TrainingMeteors)if(Job.Caster.IsValid()&&Job.Caster->IsEnemy(P))F.Threats.Add({Job.Target,FVector::ZeroVector,170*FMath::Sqrt(6.f+Job.Level),Job.Remaining,true});}
        else for(TActorIterator<AWarlockSpellFX> It(GetWorld());It;++It)if(It->Style==4&&It->Source&&It->Source->IsEnemy(P))F.Threats.Add({It->GetActorLocation(),FVector::ZeroVector,It->Size,FMath::Max(0.f,It->Duration-It->Age),true});
        if(M->Arena)for(int I=0;I<M->Arena->Pillars.Num();++I)if(M->Arena->PillarHealth[I]>0)F.Pillars.Add(M->Arena->Pillars[I]);
        ExpertFrames.Add(MoveTemp(F));
    }
    bool Fresh=false;
    while(!ExpertFrames.IsEmpty()&&ExpertFrames[0].Ready<=ExpertClock+UE_SMALL_NUMBER){ExpertView=MoveTemp(ExpertFrames[0]);ExpertFrames.RemoveAt(0);Fresh=true;}
    if(!Fresh||ExpertClock<ExpertNextAction||Tactic!=0)return;
    ExpertReactionAge=ExpertClock-ExpertView.Captured;
    if(ExpertReactionAge<.18f)return; // fail closed, including blink/new projectile snapshots
    ExpertMinimumReaction=FMath::Min(ExpertMinimumReaction,ExpertReactionAge);
    ObservedTarget=ExpertView.Target;ObservedTargetPosition=ExpertView.Position;ObservedVelocity=ExpertView.Velocity;bHasObservation=ExpertView.Target.IsValid();
    if(!bHasObservation){P->MoveInput=-O.GetSafeNormal2D();return;}
    O=ExpertView.SelfPosition+ExpertView.SelfVelocity*FMath::Min(ExpertReactionAge,.35f);
    float Age=ExpertReactionAge,Radius=FMath::Max(80.f,ExpertView.Radius),Speed=P->MoveSpeed(),Margin=FMath::Min(170.f,Radius*.25f)+FMath::Min(150.f,ExpertView.SelfKnockback);
    FVector Target=ExpertView.Position+ExpertView.Velocity*FMath::Min(Age,.35f),Delta=Target-O,D=Delta.GetSafeNormal2D();float Distance=Delta.Size2D();
    float Scores[9]={};FVector Moves[9];float BestMove=-MAX_flt;int Safest=8;
    bObservedThreat=false;
    for(int I=0;I<9;++I)
    {
        Moves[I]=I==8?FVector::ZeroVector:FVector(FMath::Cos(I*PI/4),FMath::Sin(I*PI/4),0);
        float Score=0;
        for(float H:{.18f,.4f,.7f})
        {
            FVector Next=O+Moves[I]*Speed*H+ExpertView.SelfMomentum*(H/.03f)*.18f;
            float Edge=Next.Size2D()-(Radius-Margin);if(Edge>0)Score-=4.f+Edge/30;
            for(auto Pillar:ExpertView.Pillars){float Dist=FVector::Dist2D(Next,Pillar);if(Dist<100)Score-=(100-Dist)/15;}
            for(auto& T:ExpertView.Threats)if(T.Life>Age)
            {
                FVector Start=T.Position+T.Velocity*Age,End=Start+T.Velocity*H;float HitTime;
                if(T.bArea){if(FVector::Dist2D(Next,End)<T.Radius+65)Score-=5.f;}
                else if(WarlockRules::SweepCircle(Start-O,End-Next,FVector::ZeroVector,T.Radius+65,HitTime))Score-=6.f;
            }
        }
        // Approach from the centre-facing side, pressuring the opponent toward lava.
        FVector Pressure=Target-Target.GetSafeNormal2D()*FMath::Min(420.f,Radius*.4f);
        float Preferred=P->StealthRemaining>0?70.f:420.f;
        Score-=FMath::Abs(FVector::Dist2D(O+Moves[I]*Speed*.5f,Target)-Preferred)/250;
        Score-=FVector::Dist2D(O+Moves[I]*Speed*.5f,Pressure)/1800;
        Scores[I]=Score;if(Score>BestMove){BestMove=Score;Safest=I;}
    }
    for(auto& T:ExpertView.Threats){float HitTime;if(T.Life>Age&&(T.bArea?FVector::Dist2D(O,T.Position)<T.Radius+100:WarlockRules::SweepCircle(T.Position+T.Velocity*Age,T.Position+T.Velocity*(Age+.65f),O,T.Radius+80,HitTime))){bObservedThreat=true;break;}}
    if(bObservedThreat){++ExpertDodges;if(M->bTraining)++M->TrainingDodges;}
    LearnExpert();ExpertFeatures(ExpertExperience.X);bool Mask[61]={};float Prior[61]={};
    bool Commit=ExpertClock<ExpertDodgeUntil,SafeCommitted=false;
    if(Commit)for(int I=0;I<9;++I)if(Scores[I]>=BestMove-1.1f&&FVector::DotProduct(Moves[I],ExpertDodgeDirection)>.4f)SafeCommitted=true;
    for(int I=0;I<9;++I){Mask[15+I]=Scores[I]>=BestMove-1.1f&&(!Commit||!SafeCommitted||FVector::DotProduct(Moves[I],ExpertDodgeDirection)>.4f);Prior[15+I]=Scores[I]-BestMove;}
    for(int I=24;I<27;++I){Mask[I]=true;Prior[I]=I==25?.4f:0;}
    float R=FMath::Max(.2f,1+.12f*(P->RangeRank()-P->State()->Items[7]));
    for(int I=0;I<8;++I)
    {
        int F=WarlockCatalog::Family(P->State(),I);bool Alt=P->State()->Modes[I];
        Mask[I]=P->State()->Level(I)>0&&P->Cooldowns[I]<=0&&Distance<Reach(F,Alt,R)&&P->Protection<=0;
        if(I==1)Mask[I]&=Distance<235;
        if(F==WarlockCatalog::Reflect||F==WarlockCatalog::Haste)Mask[I]&=bObservedThreat||ExpertView.SelfKnockback>22;
        if(F==WarlockCatalog::Blink||F==WarlockCatalog::Dash)Mask[I]=P->State()->Level(I)>0&&P->Cooldowns[I]<=0&&(O.Size2D()>Radius-Margin||bObservedThreat||Distance>600);
        if(F==WarlockCatalog::Dash&&ExpertView.Knockback>30&&Distance>180&&Distance<700&&Target.Size2D()<Radius-Margin)Mask[I]=P->State()->Level(I)>0&&P->Cooldowns[I]<=0;
        if(F==WarlockCatalog::Swap&&!Alt)Mask[I]&=Target.Size2D()<Radius-Margin;
        if(F==WarlockCatalog::WindWalk)Mask[I]&=P->StealthRemaining<=0;
        if(F==WarlockCatalog::Mirror)Mask[I]&=P->MirrorRemaining<=0&&P->MirrorPending<=0;
        if(F==WarlockCatalog::Rewind)Mask[I]&=P->RewindRemaining<=0;
        if(F==WarlockCatalog::Rewind&&P->RewindRemaining>0&&P->RewindPosition.Size2D()<Radius-Margin)Mask[I]=ExpertView.SelfHealth<P->RewindHealth-12||ExpertView.SelfKnockback>P->RewindKnockback+15||O.Size2D()>Radius-Margin;
        if(F==WarlockCatalog::Blink&&P->SecondBlink>0)Mask[I]=O.Size2D()>Radius-Margin||bObservedThreat;
        if(P->SilenceRemaining.IsValidIndex(I)&&P->SilenceRemaining[I]>0)Mask[I]=false;
        Prior[I]=I==0?.4f:I==1?1.8f:I==6?.1f:.7f;
        if(F==WarlockCatalog::Bind||F==WarlockCatalog::Control)Prior[I]+=.5f;
    }
    Mask[8]=Distance>180&&Distance<750&&FeintCooldown<=0&&!bObservedThreat;Prior[8]=-.9f;
    Mask[9]=WarlockCatalog::Family(P->State(),3)==WarlockCatalog::Blink&&P->State()->Level(3)>0&&P->Cooldowns[3]<=0&&P->Cooldowns[1]<=0&&Distance>330&&Distance<700;Prior[9]=.6f;
    Mask[10]=true;Prior[10]=-1.f;
    // Modes are shop actions only, just like the player's purchase rules.
    FWarlockExpertPolicy Zero;auto& Model=bDisablePolicy?Zero:ExpertPolicy();
    int Move=Model.Choose(15,9,ExpertExperience.X,Mask,Prior,Random,bLearning,ExpertExperience.Prob),Aim=Model.Choose(24,3,ExpertExperience.X,Mask,Prior,Random,bLearning,ExpertExperience.Prob);
    P->MoveInput=Moves[Move-15];P->bClickMoving=false;
    if(bObservedThreat&&(!Commit||!SafeCommitted)){ExpertDodgeDirection=P->MoveInput;ExpertDodgeUntil=ExpertClock+.55f+FMath::Min(.2f,P->CastRemaining);}
    int Action=Model.Choose(0,15,ExpertExperience.X,Mask,Prior,Random,bLearning,ExpertExperience.Prob);
    ExpertExperience.Action[0]=Action;ExpertExperience.Action[1]=Move;ExpertExperience.Action[2]=Aim;ExpertExperience.Score=bLearning?LearningScore():0;
    ++ExpertDecisions;++PolicyDecisions;LastPolicyAction=Action;ExpertNextAction=ExpertClock+.18f;
    if(P->PendingCast>=0)
    {
        if(bObservedThreat&&P->CastRemaining>.18f){P->CancelCast();++ExpertFeints;}
        return;
    }
    float Flight=Distance/750.f,Windup=Action<8?GetDefault<UWarlockTuning>()->Windup(Action):.36f;
    if(Action==2){int F=WarlockCatalog::Family(P->State(),2);Flight=Distance/(F==WarlockCatalog::Lightning?1400.f:800.f);}
    if(Action==5&&WarlockCatalog::Family(P->State(),5)==WarlockCatalog::Meteor&&!P->State()->Modes[5])Flight=1.15f;
    float Lead=Aim==24?Age:Aim==25?Age+Windup+Flight:Age+(Windup+Flight)*.55f;
    P->AimPoint=ExpertView.Position+ExpertView.Velocity*FMath::Min(1.4f,Lead);
    P->AimPoint=P->AimPoint.GetClampedToMaxSize2D(Radius+120);
    if(Action==8){if(TryFeint()){++ExpertFeints;if(M->bTraining)++M->TrainingFeints;}return;}
    if(Action==9){TryBlinkSmite();return;}
    if(Action>=11){int Slot=Action==11?3:Action==12?4:Action==13?5:7;FString Reason;M->SetSkillMode(P,WarlockCatalog::Product(WarlockCatalog::Family(P->State(),Slot)),Reason);ExpertNextMode=ExpertClock+6;return;}
    if(Action==10)return;
    int Family=WarlockCatalog::Family(P->State(),Action);
    if(Family==WarlockCatalog::Blink||Family==WarlockCatalog::Dash)P->AimPoint=(O+Moves[Safest]*650).GetClampedToMaxSize2D(FMath::Max(30.f,Radius-110));
    if(Family==WarlockCatalog::Dash&&!bObservedThreat&&ExpertView.Knockback>30&&Target.Size2D()<Radius-Margin)P->AimPoint=Target;
    if(Family==WarlockCatalog::Mirror&&P->MirrorPending>0)P->FormMirrors(P->AimPoint);
    if(P->Cast(Action)&&M->bTraining&&Family>=0&&Family<23)++M->TrainingFamilyCasts[Family];
}
