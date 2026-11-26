#include "WarlockAnimation.h"
#include "WarlockGame.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "AnimNodes/AnimNode_LayeredBoneBlend.h"

struct FWarlockAnimationProxy final : FAnimInstanceProxy
{
    FAnimNode_SequencePlayer_Standalone Idle,Walk,Preparation,Cast,Death;
    FAnimNode_TwoWayBlend MoveBlend,GestureBlend,FullBodyCast,DeathBlend;
    FAnimNode_LayeredBoneBlend UpperBody;
    int32 SeenCast=0;
    float CastAge=100,CastLength=0;
    float ReleaseAge=1,ContactTime=0;
    float PreparationLength=0;
    bool WasCasting=false;
    bool WasAlive=true;

    explicit FWarlockAnimationProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance)
    {
        MoveBlend.A.SetLinkNode(&Idle);MoveBlend.B.SetLinkNode(&Walk);
        GestureBlend.A.SetLinkNode(&Cast);GestureBlend.B.SetLinkNode(&Preparation);
        UpperBody.AddPose();UpperBody.BasePose.SetLinkNode(&MoveBlend);UpperBody.BlendPoses[0].SetLinkNode(&GestureBlend);
        FBranchFilter Branch;Branch.BoneName=TEXT("36_Bone_Chest");Branch.BlendDepth=0;
        UpperBody.LayerSetup[0].BranchFilters.Add(Branch);UpperBody.bMeshSpaceRotationBlend=true;
        FullBodyCast.A.SetLinkNode(&UpperBody);FullBodyCast.B.SetLinkNode(&GestureBlend);
        DeathBlend.A.SetLinkNode(&FullBodyCast);DeathBlend.B.SetLinkNode(&Death);
        Idle.SetLoopAnimation(true);Walk.SetLoopAnimation(true);Preparation.SetLoopAnimation(false);Cast.SetLoopAnimation(false);Death.SetLoopAnimation(false);
    }
    virtual void Initialize(UAnimInstance* Instance) override
    {
        if(auto P=CastChecked<AWarlockPawn>(Instance->TryGetPawnOwner()))
        {
            Idle.SetSequence(P->IdleAnimation);Walk.SetSequence(P->WalkAnimation);
            Cast.SetSequence(P->AttackAnimation);Death.SetSequence(P->DeathAnimation);SeenCast=P->CastSerial;
            Preparation.SetSequence(P->SpellAnimation);PreparationLength=P->SpellAnimation?P->SpellAnimation->GetPlayLength():0;Preparation.SetPlayRate(0);
        }
        FAnimInstanceProxy::Initialize(Instance);
    }
    // A native graph has no Blueprint compiled NodeData; FAnimNode_Root's trace name
    // reads that data in cooked builds. The final native blend is the root instead.
    virtual FAnimNode_Base* GetCustomRootNode() override{return &DeathBlend;}
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override
    {OutNodes={&Idle,&Walk,&Preparation,&Cast,&Death,&MoveBlend,&GestureBlend,&UpperBody,&FullBodyCast,&DeathBlend};}
    virtual void PreUpdate(UAnimInstance* Instance,float Dt) override
    {
        FAnimInstanceProxy::PreUpdate(Instance,Dt);
        auto P=::Cast<AWarlockPawn>(Instance->TryGetPawnOwner());if(!P)return;
        const float Target=P->bAlive?FMath::Clamp(P->LocomotionSpeed/210.f,0.f,1.f):0;
        MoveBlend.Alpha=FMath::FInterpTo(MoveBlend.Alpha,Target,Dt,12);
        Walk.SetPlayRate(FMath::Max(.3f,P->LocomotionSpeed/210.f));
        if(SeenCast!=P->CastSerial)
        {
            SeenCast=P->CastSerial;
            const int Slot=P->LastCastSlot;
            if(Slot<8)
            {
                CastAge=0;ReleaseAge=0;WasCasting=true;
                // Original aani uses death for the projectile gestures; Spell has
                // the clearly raised staff pose shared by every anticipation.
                UAnimSequence* Sequence=Slot==1||Slot==5?P->SlamAnimation:Slot==0||Slot==4||Slot==7?P->DeathAnimation:P->SpellAnimation;
                Cast.SetSequence(Sequence);CastLength=Sequence?Sequence->GetPlayLength():0;
                ContactTime=CastLength*(Slot==0||Slot==4||Slot==7?.12f:Slot==1||Slot==5?.40f:Slot==2?.65f:.45f);
                const float Starts[]={.16f,.12f,.20f,.15f,.17f,.10f,.22f,.16f};
                GestureBlend.Alpha=1;Preparation.SetAccumulatedTime(PreparationLength*Starts[Slot]);
                Cast.SetPlayRate(0);
            }
            else {WasCasting=false;ReleaseAge=.5f;}
        }
        CastAge+=Dt;
        bool Pending=P->LastCastSlot<8&&P->PendingCast==P->LastCastSlot&&P->CastDuration>0;
        if(Pending)
        {
            float Progress=1-P->CastRemaining/FMath::Max(.001f,P->CastDuration);
            float Raise=FMath::SmoothStep(0.f,.65f,FMath::Clamp(Progress,0.f,1.f));
            float RaisedFraction=P->LastCastSlot==1||P->LastCastSlot==5?.48f:.40f;
            const float Starts[]={.16f,.12f,.20f,.15f,.17f,.10f,.22f,.16f};
            Preparation.SetAccumulatedTime(PreparationLength*FMath::Lerp(Starts[P->LastCastSlot],RaisedFraction,Raise));GestureBlend.Alpha=1;WasCasting=true;
        }
        else
        {
            if(WasCasting){ReleaseAge=0;WasCasting=false;}
            ReleaseAge+=Dt;
            // A cancelled pose fades out in place; it must not play a release.
            if(P->LastCastSlot<8){Cast.SetAccumulatedTime(FMath::Min(CastLength*.88f,ContactTime+ReleaseAge*1.1f));GestureBlend.Alpha=FMath::FInterpTo(GestureBlend.Alpha,0.f,Dt,22);}
        }
        float TargetCast=P->bAlive&&P->LastCastSlot<8&&(Pending||ReleaseAge<.16f||(P->LastCastSlot==4&&P->ShotsLeft>0))?1.f:0.f;
        UpperBody.BlendWeights[0]=FMath::FInterpTo(UpperBody.BlendWeights[0],TargetCast,Dt,Pending?14.f:11.f);
        const float BodyWeights[]={.22f,.85f,.35f,.18f,.28f,.80f,.42f,.30f};
        float WholeBody=P->LocomotionSpeed<5&&P->LastCastSlot<8?UpperBody.BlendWeights[0]*BodyWeights[P->LastCastSlot]:0;
        FullBodyCast.Alpha=FMath::FInterpTo(FullBodyCast.Alpha,WholeBody,Dt,12);
        if(WasAlive&&!P->bAlive)Death.SetAccumulatedTime(0);
        DeathBlend.Alpha=P->bAlive?0.f:1.f;WasAlive=P->bAlive;
        auto Animation=CastChecked<UWarlockAnimation>(Instance);Animation->LocomotionBlend=MoveBlend.Alpha;Animation->CastingBlend=UpperBody.BlendWeights[0];Animation->bPlayingDeath=!P->bAlive;
    }
};
FAnimInstanceProxy* UWarlockAnimation::CreateAnimInstanceProxy(){return new FWarlockAnimationProxy(this);}
void UWarlockAnimation::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy){delete Proxy;}
