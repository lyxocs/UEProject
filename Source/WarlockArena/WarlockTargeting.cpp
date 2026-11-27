#include "WarlockGame.h"
#include "WarlockUserSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

void AWarlockController::CancelTargeting()
{
    AimingSlot=MouseHeldSlot=AimFeedbackSlot=-1;AimFeedbackTime=0;
    if(IsValid(AimGuide))AimGuide->SetVisibility(false);
}
void AWarlockController::StartTargeting(int Slot)
{
    auto P=Fighter();auto M=GetWorld()->GetGameState<AWarlockMatchState>();
    if(!P||!P->bAlive||!P->State()||Slot<0||Slot>7||bShopOpen||bHelpOpen||UGameplayStatics::IsGamePaused(this)||!M||(M->Phase!=TEXT("SHOP")&&M->Phase!=TEXT("FIGHT")))return;
    if(P->State()->Level(Slot)<=0){SetNotice(TEXT("尚未学习该技能"));return;}
    if(P->PendingCast>=0)return;
    bool Second=Slot==3&&P->State()->Modes[3]&&P->SecondBlink>0;
    bool Detach=Slot==7&&P->ChainTime>0;
    if(!Second&&!Detach&&P->Cooldowns[Slot]>.001f){SetNotice(TEXT("技能尚未冷却"));return;}
    if(bGamepadAiming||!UsesMouse())LocalAim=P->GetActorLocation()+LastAimDirection*1200;
    auto Settings=Cast<UWarlockUserSettings>(UGameUserSettings::GetGameUserSettings());
    if(UsesTargetGuide(Slot)&&!Detach&&(!Settings||Settings->CastTriggerMode==1))
    {AimingSlot=Slot;AimFeedbackSlot=-1;AimFeedbackTime=0;UpdateTargetGuide(0);return;}
    AimingSlot=-1;ConfirmedAim=LocalAim;AimFeedbackSlot=UsesTargetGuide(Slot)?Slot:-1;AimFeedbackTime=.16f;
    ServerCast(Slot,LocalAim);
}
void AWarlockController::ReleaseTargeting(int Slot)
{
    if(AimingSlot!=Slot||Slot<0)return;
    if(bShopOpen||bHelpOpen||UGameplayStatics::IsGamePaused(this)||!Fighter()||!Fighter()->bAlive){CancelTargeting();return;}
    AimingSlot=-1;MouseHeldSlot=-1;ConfirmedAim=LocalAim;AimFeedbackSlot=Slot;AimFeedbackTime=.16f;
    ServerCast(Slot,LocalAim);
}
void AWarlockController::MousePrimaryReleased()
{int Slot=MouseHeldSlot;MouseHeldSlot=-1;if(Slot>=0)ReleaseTargeting(Slot);}
void AWarlockController::UpdateTargetGuide(float Dt)
{
    auto P=Fighter();auto State=P?P->State():nullptr;
    auto Match=GetWorld()->GetGameState<AWarlockMatchState>();
    if(!P||!State||!P->bAlive||bShopOpen||bHelpOpen||UGameplayStatics::IsGamePaused(this)||!Match||(Match->Phase!=TEXT("SHOP")&&Match->Phase!=TEXT("FIGHT")))
    {if(AimingSlot>=0||AimFeedbackSlot>=0)CancelTargeting();return;}
    AimFeedbackTime=FMath::Max(0.f,AimFeedbackTime-Dt);
    int Slot=AimingSlot>=0?AimingSlot:AimFeedbackTime>0?AimFeedbackSlot:-1;
    if(!UsesTargetGuide(Slot)){if(IsValid(AimGuide))AimGuide->SetVisibility(false);return;}
    if(!IsValid(AimGuide)||AimGuide->GetOwner()!=P)
    {
        if(IsValid(AimGuide))AimGuide->DestroyComponent();
        AimGuide=NewObject<UStaticMeshComponent>(P);AimGuide->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
        AimGuide->SetCollisionEnabled(ECollisionEnabled::NoCollision);AimGuide->SetCastShadow(false);AimGuide->SetOnlyOwnerSee(true);AimGuide->RegisterComponent();
        auto Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WarlockArena/Materials/M_AimGuide.M_AimGuide"));
        if(Base){AimGuideMaterial=UMaterialInstanceDynamic::Create(Base,this);AimGuide->SetMaterial(0,AimGuideMaterial);}
    }
    if(!AimGuideMaterial){AimGuide->SetVisibility(false);return;}
    FVector Origin=P->GetActorLocation(),Target=AimingSlot>=0?LocalAim:ConfirmedAim;
    FVector Direction=(Target-Origin).GetSafeNormal2D();if(Direction.IsNearlyZero())Direction=LastAimDirection;
    float Range=1+.12f*(State->RangeMastery-State->Items[7]);
    float Reach=Slot==2?600*Range:Slot==7?817*Range:Slot==4?(State->Modes[4]?675.f:1000.f)*Range:1000*Range;
    float Width=Slot==0?50.f:Slot==2?100.f:Slot==7?70.f:Slot==4?Reach*.64f:50.f;
    if(Slot==4){float Count=5+State->Level(4);float HalfAngle=State->Modes[4]?FMath::DegreesToRadians(10.35f):.32f*(Count-2)/Count;Width=2*Reach*FMath::Tan(HalfAngle);}
    int Shape=Slot==4?1:0;
    FVector Center=Origin+Direction*(Reach*.5f);
    if(Slot==3||Slot==5)
    {
        Shape=2;Reach=Slot==5?1000:State->Modes[3]?(P->SecondBlink>0?300.f:300.f+50*State->Level(3)):700.f+70*State->Level(3);
        Center=Origin+(Target-Origin).GetClampedToMaxSize2D(Reach);
        if(Slot==3)for(TActorIterator<AWarlockArena> It(GetWorld());It;++It)for(int I=0;I<It->Pillars.Num();++I)if(It->PillarHealth.IsValidIndex(I)&&It->PillarHealth[I]>0&&FVector::Dist2D(Center,It->Pillars[I])<65)Center=It->Pillars[I]+(Center-It->Pillars[I]).GetSafeNormal2D()*66;
        Width=Slot==3?90.f:170.f*FMath::Sqrt(6.f+State->Level(5));Reach=Width;
    }
    Center.Z=5;
    AimGuide->SetWorldLocation(Center);AimGuide->SetWorldRotation(FRotator(0,Shape==2?0:Direction.Rotation().Yaw,0));
    AimGuide->SetWorldScale3D(FVector(Reach/100,Width/100,1));
    AimGuideMaterial->SetScalarParameterValue(TEXT("Shape"),Shape);
    AimGuideMaterial->SetScalarParameterValue(TEXT("Opacity"),AimingSlot>=0?1.f:AimFeedbackTime/.16f);
    AimGuideMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.36f,.63f,.68f));
    AimGuide->SetVisibility(true);
}
void AWarlockController::ScrollShop(int Rows)
{if(bShopOpen&&!(SelectedProduct>=12&&SelectedProduct<20))ShopTopRow=FMath::Clamp(ShopTopRow+Rows,0,8);}
void AWarlockController::SelectShopProduct(int Product)
{
    SelectedProduct=FMath::Clamp(Product,0,20);
    if(SelectedProduct<12||SelectedProduct==20)
    {int Row=SelectedProduct==20?6:SelectedProduct;if(Row<ShopTopRow)ShopTopRow=Row;else if(Row>=ShopTopRow+4)ShopTopRow=Row-3;ShopTopRow=FMath::Clamp(ShopTopRow,0,8);}
}
