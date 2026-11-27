#include "WarlockGame.h"
#include "Engine/World.h"
#include "WarlockCatalog.h"

bool AWarlockAI::TryFeint()
{
    auto P=Cast<AWarlockPawn>(GetPawn());auto Target=ObservedTarget.Get();
    if(!P||!P->State()||!Target||!Target->bAlive||!P->IsEnemy(Target)||!P->bAlive||!bHasObservation||P->Protection>0||P->PendingCast>=0||FeintCooldown>0||Tactic!=0)return false;
    float Distance=FVector::Dist2D(P->GetActorLocation(),ObservedTargetPosition);
    int Slot=Distance<360?1:Distance<600&&P->State()->Level(2)>0?2:0;if(Distance<180||Distance>750||P->State()->Level(Slot)<=0||P->Cooldowns[Slot]>0)return false;
    P->AimPoint=ObservedTargetPosition;if(!P->Cast(Slot))return false;
    Tactic=1;TacticCastSerial=P->CastSerial;TacticWait=Random.FRandRange(.20f,.32f);FeintCooldown=Random.FRandRange(9.f,14.f);return true;
}
bool AWarlockAI::TryBlinkSmite()
{
    auto P=Cast<AWarlockPawn>(GetPawn());auto Target=ObservedTarget.Get();auto Mode=GetWorld()->GetAuthGameMode<AWarlockMode>();
    if(P&&WarlockCatalog::Family(P->State(),3)!=WarlockCatalog::Blink)return false;
    if(!P||!P->State()||!Target||!Target->bAlive||!P->IsEnemy(Target)||!Mode||!P->bAlive||!bHasObservation||P->Protection>0||P->PendingCast>=0||Tactic!=0||ComboCooldown>0||P->State()->Level(3)<=0||P->Cooldowns[3]>0||P->Cooldowns[1]>0)return false;
    FVector Delta=ObservedTargetPosition-P->GetActorLocation();float Distance=Delta.Size2D();if(Distance<330||Distance>700)return false;
    FVector Landing=ObservedTargetPosition-Delta.GetSafeNormal2D()*135;
    if(Landing.Size2D()>Mode->Match()->ArenaRadius-110)return false;
    TacticTarget=ObservedTargetPosition;P->AimPoint=Landing;TacticBlinkSerial=P->BlinkSerial;
    if(!P->Cast(3))return false;
    Tactic=2;TacticWait=.6f;ComboCooldown=Random.FRandRange(11.f,17.f);return true;
}
void AWarlockAI::UpdateTactics(float Dt)
{
    FeintCooldown=FMath::Max(0.f,FeintCooldown-Dt);ComboCooldown=FMath::Max(0.f,ComboCooldown-Dt);StrafeTimer-=Dt;
    if(StrafeTimer<=0){StrafeTimer=Random.FRandRange(2.f,4.f);StrafeSign=Random.FRand()<.5f?-1.f:1.f;}
    auto P=Cast<AWarlockPawn>(GetPawn());if(!P||!P->bAlive){Tactic=0;return;}if(Tactic==0)return;
    TacticWait-=Dt;
    if(Tactic==1)
    {
        if(P->CastSerial!=TacticCastSerial||P->PendingCast<0){Tactic=0;return;}
        if(TacticWait<=0){P->CancelCast();Tactic=0;NextDecision=DecisionTime()+.3f;}return;
    }
    if(Tactic==2)
    {
        if(P->BlinkSerial!=TacticBlinkSerial){Tactic=3;TacticWait=Difficulty().ReactionMin<.2f?Random.FRandRange(.18f,.24f):Random.FRandRange(.12f,.22f);return;}
        if(P->PendingCast!=3||TacticWait<=0){Tactic=0;return;}
    }
    else if(Tactic==3&&TacticWait<=0)
    {
        auto Mode=GetWorld()->GetAuthGameMode<AWarlockMode>();Tactic=0;
        // Commit to the previously observed position; do not track an unseen dodge.
        if(Mode&&P->GetActorLocation().Size2D()<Mode->Match()->ArenaRadius-70&&FVector::Dist2D(P->GetActorLocation(),TacticTarget)<250)
        {P->AimPoint=TacticTarget;P->Cast(1);NextDecision=DecisionTime()+.5f;}
    }
}
