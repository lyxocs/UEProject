#include "WarlockAnimation.h"
#include "WarlockGame.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "AnimNodes/AnimNode_LayeredBoneBlend.h"

struct FWarlockAnimationProxy final : FAnimInstanceProxy
{
    FAnimNode_SequencePlayer_Standalone Idle,Walk,Cast,Death;
    FAnimNode_TwoWayBlend MoveBlend,FullBodyCast,DeathBlend;
    FAnimNode_LayeredBoneBlend UpperBody;
    int32 SeenCast=0;
    float CastAge=100,CastLength=0;
    float ReleaseAge=1,ContactTime=0;
    bool WasCasting=false;
    bool WasAlive=true;

    explicit FWarlockAnimationProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance)
    {
        MoveBlend.A.SetLinkNode(&Idle);MoveBlend.B.SetLinkNode(&Walk);
        UpperBody.AddPose();UpperBody.BasePose.SetLinkNode(&MoveBlend);UpperBody.BlendPoses[0].SetLinkNode(&Cast);
        FBranchFilter Branch;Branch.BoneName=TEXT("36_Bone_Chest");Branch.BlendDepth=0;
        UpperBody.LayerSetup[0].BranchFilters.Add(Branch);UpperBody.bMeshSpaceRotationBlend=true;
        FullBodyCast.A.SetLinkNode(&UpperBody);FullBodyCast.B.SetLinkNode(&Cast);
        DeathBlend.A.SetLinkNode(&FullBodyCast);DeathBlend.B.SetLinkNode(&Death);
        Idle.SetLoopAnimation(true);Walk.SetLoopAnimation(true);Cast.SetLoopAnimation(false);Death.SetLoopAnimation(false);
    }
    virtual void Initialize(UAnimInstance* Instance) override
    {
        if(auto P=CastChecked<AWarlockPawn>(Instance->TryGetPawnOwner()))
        {
            Idle.SetSequence(P->IdleAnimation);Walk.SetSequence(P->WalkAnimation);
            Cast.SetSequence(P->AttackAnimation);Death.SetSequence(P->DeathAnimation);SeenCast=P->CastSerial;
        }
        FAnimInstanceProxy::Initialize(Instance);
    }
    // A native graph has no Blueprint compiled NodeData; FAnimNode_Root's trace name
    // reads that data in cooked builds. The final native blend is the root instead.
    virtual FAnimNode_Base* GetCustomRootNode() override{return &DeathBlend;}
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override
    {OutNodes={&Idle,&Walk,&Cast,&Death,&MoveBlend,&UpperBody,&FullBodyCast,&DeathBlend};}
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
                UAnimSequence* Sequence=Slot==1||Slot==5?P->SlamAnimation:Slot==2||Slot==3||Slot==6?P->SpellAnimation:P->AttackAnimation;
                Cast.SetSequence(Sequence);CastLength=Sequence?Sequence->GetPlayLength():0;
                ContactTime=CastLength*(Slot==1||Slot==5?.46f:Slot==2||Slot==6?.32f:.4f);
                Cast.SetPlayRate(0);
            }
            else {WasCasting=false;ReleaseAge=.5f;}
        }
        CastAge+=Dt;
        bool Pending=P->LastCastSlot<8&&(P->PendingCast==P->LastCastSlot||(P->CastDuration<=0&&CastAge<CastLength*.55f));
        if(Pending)
        {
            float Progress=P->CastDuration>0?1-P->CastRemaining/FMath::Max(.001f,P->CastDuration):CastAge/FMath::Max(.001f,CastLength*.55f);
            Cast.SetAccumulatedTime(ContactTime*FMath::InterpEaseInOut(0.f,1.f,FMath::Clamp(Progress,0.f,1.f),1.5f));WasCasting=true;
        }
        else
        {
            if(WasCasting){ReleaseAge=0;WasCasting=false;}
            ReleaseAge+=Dt;if(P->LastCastSlot<8)Cast.SetAccumulatedTime(FMath::Min(CastLength,ContactTime+ReleaseAge*.75f));
        }
        float TargetCast=P->bAlive&&P->LastCastSlot<8&&(Pending||ReleaseAge<.1f)?1.f:0.f;
        UpperBody.BlendWeights[0]=FMath::FInterpTo(UpperBody.BlendWeights[0],TargetCast,Dt,Pending?14.f:11.f);
        float WholeBody=(P->LastCastSlot==1||P->LastCastSlot==5)&&P->LocomotionSpeed<5?UpperBody.BlendWeights[0]*.85f:0;
        FullBodyCast.Alpha=FMath::FInterpTo(FullBodyCast.Alpha,WholeBody,Dt,12);
        if(WasAlive&&!P->bAlive)Death.SetAccumulatedTime(0);
        DeathBlend.Alpha=P->bAlive?0.f:1.f;WasAlive=P->bAlive;
        auto Animation=CastChecked<UWarlockAnimation>(Instance);Animation->LocomotionBlend=MoveBlend.Alpha;Animation->CastingBlend=UpperBody.BlendWeights[0];Animation->bPlayingDeath=!P->bAlive;
    }
};
FAnimInstanceProxy* UWarlockAnimation::CreateAnimInstanceProxy(){return new FWarlockAnimationProxy(this);}
void UWarlockAnimation::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy){delete Proxy;}
