#include "WarlockGame.h"
#include "WarlockPainter.h"
#include "WarlockPresentation.h"
#include "WarlockUserSettings.h"
#include "Misc/App.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const TCHAR* Names[]={TEXT("火球"),TEXT("天罚"),TEXT("闪电"),TEXT("瞬间移动"),TEXT("火焰喷射"),TEXT("流星"),TEXT("反射盾"),TEXT("锁链"),TEXT("生命精通"),TEXT("远程精通"),TEXT("时间精通"),TEXT("博而不专"),TEXT("抗魔斗篷"),TEXT("熔岩靴"),TEXT("鲜血之剑"),TEXT("乔丹之石"),TEXT("速度之靴"),TEXT("恢复之戒"),TEXT("血牌"),TEXT("头盔"),TEXT("急行")};

}
AWarlockHUD::AWarlockHUD()
{
    static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Engine/EngineFonts/Roboto.Roboto"));InterfaceFont=Font.Object;
}
void AWarlockHUD::DrawHUD()
{
    Super::DrawHUD();if(!Canvas||!InterfaceFont)return;
    auto C=Cast<AWarlockController>(PlayerOwner);auto P=C?C->Fighter():nullptr;auto S=P?P->State():nullptr;auto M=GetWorld()->GetGameState<AWarlockMatchState>();if(!C||!P||!S||!M)return;
    using namespace WarlockUI;
    const float DesignWidth=C->bShopOpen&&Canvas->SizeX<Canvas->SizeY*1.2f?1000.f:1280.f;
    float Scale=FMath::Clamp(FMath::Min(Canvas->SizeX/DesignWidth,Canvas->SizeY/620.f),.55f,3.5f),W=Canvas->SizeX/Scale,H=Canvas->SizeY/Scale;
    Painter U{this,Canvas,InterfaceFont,Scale};
    const bool Compact=M->Phase!=TEXT("SHOP");
    auto Palette=UWarlockPresentation::Palette(UWarlockPresentation::PaletteForRound(M->Round,M->Phase));
    auto DrawPerformance=[&]()
    {
        float Dt=FMath::Clamp(float(FApp::GetDeltaTime()),.0001f,2.f);
        SmoothedFrameSeconds=SmoothedFrameSeconds>0?FMath::Lerp(SmoothedFrameSeconds,Dt,1-FMath::Exp(-Dt*4)):Dt;
        float FPS=1.f/SmoothedFrameSeconds;
        U.Round(W-179,66,163,25,5,Alpha(Ink,.82f));
        U.Text(FString::Printf(TEXT("%.0f FPS  ·  %.1f ms"),FPS,SmoothedFrameSeconds*1000),W-97,72,13,FPS>=55?FLinearColor(.55f,.8f,.65f):FPS>=30?Gold:FLinearColor(.95f,.42f,.27f),true);
    };
    if(UGameplayStatics::IsGamePaused(this))
    {
        if(C->bDisplaySettingsOpen)
        {
            auto Settings=Cast<UWarlockUserSettings>(UGameUserSettings::GetGameUserSettings());
            auto First=Cast<AWarlockController>(GetWorld()->GetFirstPlayerController());
            U.Rect(0,0,W,H,FLinearColor(.004f,.008f,.012f,.78f));U.Panel(W/2-350,H/2-277,700,554,16,Ink);
            U.Text(TEXT("画面与性能"),W/2,H/2-256,25,Gold,true);
            U.Text(TEXT("本机双人共享 · 即时生效并保存 · UI 分辨率不随渲染比例降低"),W/2,H/2-219,12,Muted,true);
            U.Round(W/2-170,H/2-193,340,30,6,Alpha(Gold,.12f));U.Text(TEXT("返回 AI 设置 · Tab / 手柄 X"),W/2,H/2-186,13,Ivory,true);U.Hit(W/2-170,H/2-193,340,30,TEXT("DisplayTab"));
            if(Settings)for(int I=0;I<UWarlockUserSettings::RowCount;++I)
            {
                float Y=H/2-145+I*29;bool Selected=(First?First:C)->SelectedDisplayRow==I;
                if(Selected)U.Round(W/2-322,Y-2,644,28,5,Alpha(Gold,.12f));
                U.Text(UWarlockUserSettings::RowName(I),W/2-300,Y+4,14,Selected?Gold:Muted);
                U.Text(Settings->Label(I),W/2+66,Y+4,13,Ivory,true);
                for(int D:{-1,1}){float X=W/2+(D<0?-113:278);U.Round(X,Y,30,25,4,Alpha(Gold,.13f));U.Text(D<0?TEXT("‹"):TEXT("›"),X+15,Y+2,17,Gold,true);U.Hit(X,Y,30,25,FName(*FString::Printf(TEXT("Display%d_%d"),I,D)));}
            }
            U.Text(TEXT("帧率偏低先选「流畅」；4K / 分屏可再降低场景渲染比例。"),W/2,H/2+158,12,Gold,true);
            U.Text(TEXT("↑↓ 选择 · ←→ 调整 · VSync 开启时还会受显示器刷新率限制"),W/2,H/2+179,11,Muted,true);
            U.Round(W/2-140,H/2+211,280,42,8,Alpha(Gold,.16f));U.Text(TEXT("继续游戏 · Esc / Menu"),W/2,H/2+223,16,Ivory,true);U.Hit(W/2-140,H/2+211,280,42,TEXT("Resume"));DrawPerformance();return;
        }
        U.Rect(0,0,W,H,FLinearColor(.004f,.008f,.012f,.72f));U.Panel(W/2-270,H/2-199,540,398,16,Ink);
        U.Round(W/2-170,H/2-187,340,28,6,Alpha(Gold,.12f));U.Text(TEXT("画面与性能 · Tab / 手柄 X"),W/2,H/2-180,13,Ivory,true);U.Hit(W/2-170,H/2-187,340,28,TEXT("DisplayTab"));
        U.Text(TEXT("游戏已暂停"),W/2,H/2-144,27,Gold,true);U.Text(TEXT("所有玩家、法术和回合计时已暂停"),W/2,H/2-99,14,Muted,true);
        U.Text(TEXT("AI 强度 · ")+FString(FWarlockDifficulty::Name(M->AIDifficulty)),W/2,H/2-56,18,Ivory,true);
        for(int I=0;I<5;++I){int Tier=5-I;float X=W/2-239+I*96;bool Selected=M->AIDifficulty==Tier;
            U.Round(X,H/2-17,92,42,8,Selected?Alpha(Gold,.28f):FLinearColor(.035f,.052f,.06f,.9f));U.Text(FString::Printf(TEXT("%d %s"),Tier,FWarlockDifficulty::Name(Tier)),X+46,H/2-6,16,Selected?Gold:Muted,true);U.Hit(X,H/2-17,92,42,FName(*FString::Printf(TEXT("Difficulty%d"),Tier)));}
        U.Text(TEXT("1–5 选择 · 手柄十字键左右调整 · 房主生效"),W/2,H/2+42,12,Muted,true);
        U.Text(AWarlockController::IsTrainedAIReady()?TEXT("强化学习战术策略已加载"):TEXT("基础策略 · 学习模型尚未加载"),W/2,H/2+62,10,Muted,true);
        U.Round(W/2-140,H/2+88,280,46,9,Alpha(Gold,.16f));U.Text(TEXT("继续游戏  ·  Esc / Menu"),W/2,H/2+101,17,Ivory,true);U.Hit(W/2-140,H/2+88,280,46,TEXT("Resume"));DrawPerformance();return;
    }
    for(int I=0;I<30;++I){float T=I/30.f,Bottom=Compact?76.f:180.f;U.Rect(0,H-Bottom+I*Bottom/30,W,Bottom/30,FLinearColor(.007f,.012f,.018f,T*T*.8f));U.Rect(0,I*3,W,3,FLinearColor(.007f,.012f,.018f,(1-T)*(1-T)*.65f));}
    if(Compact)U.Round(W/2-122,10,244,78,8,Alpha(Ink,.44f));else U.Panel(W/2-122,10,244,92,12,Alpha(Ink,.86f));U.Divider(W/2-88,34,176);
    U.Arc(40,36,25,-75,105,Alpha(Gold,.28f));U.Arc(40,36,25,105,105,Alpha(Gold,.28f));
    U.Icon(1,40,36,27,Gold);U.Text(TEXT("术 士 之 战"),64,23,20,Ivory);U.Text(Palette.Name+(M->bTeams?TEXT(" / 合作"):TEXT(" / 自由竞技")),64,50,11,Muted);
    int Alive=0;TArray<AWarlockPawn*> Roster;
    for(TActorIterator<AWarlockPawn> It(GetWorld());It;++It){Roster.Add(*It);Alive+=It->bAlive;}
    for(auto It=HealthDisplays.CreateIterator();It;++It)if(!It.Key().IsValid())It.RemoveCurrent();
    const float NameScale=FMath::Max(Scale,FMath::Clamp(Canvas->SizeY/900.f,.8f,1.55f));
    const float NameW=Canvas->SizeX/NameScale,NameH=Canvas->SizeY/NameScale;
    Painter NameUI{this,Canvas,InterfaceFont,NameScale};
    for(auto Other:Roster)
    {
        auto& Display=HealthDisplays.FindOrAdd(Other);float Actual=FMath::Clamp(Other->Health/FMath::Max(1.f,Other->MaxHealth()),0.f,1.f);
        if(Actual>Display.Actual){Display.Trail=FMath::Max(Display.Trail,Actual);if(Actual>=Display.Trail)Display.Hold=0;}
        else if(Actual<Display.Actual-.0001f)Display.Hold=.25f;
        Display.Actual=Actual;float Dt=GetWorld()->GetDeltaSeconds();Display.Hold=FMath::Max(0.f,Display.Hold-Dt);
        if(Display.Hold<=0)Display.Trail=FMath::FInterpTo(Display.Trail,Actual,Dt,7);
        Display.Trail=FMath::Clamp(Display.Trail,Actual,1.f);
        if(!Other->bAlive||C->bShopOpen||!Other->State())continue;
        FVector2D Screen;if(!C->ProjectWorldLocationToScreen(Other->GetActorLocation()+FVector(0,0,135),Screen,true))continue;
        float BX=Screen.X/NameScale-34,BY=Screen.Y/NameScale;
        if(BX<0||BX+68>NameW||BY<90||BY>NameH-(Compact?95:155))continue;
        bool Friendly=Other->State()->Team==S->Team;FLinearColor Color=Friendly?FLinearColor(.48f,.73f,.6f):FLinearColor(.78f,.34f,.24f);
        NameUI.Text(Other->State()->GetPlayerName(),BX+34,BY-29,10,Other==P?Gold:Ivory,true);
        NameUI.Round(BX-2,BY-2,72,10,3,Alpha(Ink,.95f));NameUI.Rect(BX,BY,68*Display.Trail,5,Alpha(Gold,.6f));NameUI.Rect(BX,BY,68*Actual,5,Color);
        NameUI.Text(FString::Printf(TEXT("击退 %.1f"),Other->Knockback),BX+34,BY-15,9,Gold,true);
    }
    FString Phase=M->Phase==TEXT("SHOP")?TEXT("准备阶段"):M->Phase==TEXT("FIGHT")?Palette.Name:M->Phase==TEXT("FINAL")?TEXT("比赛结束"):TEXT("回合结算");
    int RoundShown=M->Phase==TEXT("SHOP")?M->Round+1:FMath::Max(1,M->Round);
    U.Text(RoundShown>11?FString::Printf(TEXT("加 赛  %02d"),RoundShown-11):FString::Printf(TEXT("回 合  %02d / 11"),RoundShown),W/2,17,12,Gold,true);U.Text(Phase,W/2,38,21,Ivory,true);
    if(M->Phase==TEXT("SHOP")){
        int Ready=0,Humans=0;for(auto Other:Roster)if(auto OS=Other->State();OS&&!OS->bBot){++Humans;Ready+=OS->bReady;}
        U.Round(W/2-109,67,218,28,7,S->bReady?FLinearColor(.07f,.19f,.13f):Alpha(Gold,.17f));
        U.Text(FString::Printf(TEXT("%s · %d/%d"),S->bReady?TEXT("已准备"):TEXT("准备完毕"),Ready,Humans),W/2,73,14,S->bReady?FLinearColor(.55f,.85f,.63f):Gold,true);U.Hit(W/2-109,67,218,28,TEXT("Ready"));
        if(!C->bShopOpen)U.Text(FString::Printf(TEXT("无伤练习 · 空格/手柄↑准备 · %d秒"),FMath::CeilToInt(M->Remaining)),W/2,105,10,Muted,true);
    }
    else U.Text(FString::Printf(TEXT("%d 名术士存活"),Alive),W/2,64,10,Muted,true);
    U.Text(TEXT("F1  操作指南"),W-78,25,12,Muted,true);if(!Compact)U.Text(TEXT("F6 分辨率  ·  F7 画质"),W-112,51,10,Alpha(Muted,.8f),true);U.Hit(W-145,15,132,35,TEXT("Help"));
    U.Text(TEXT("Esc 设置 / 暂停"),W-215,25,12,Gold,true);U.Hit(W-267,15,95,29,TEXT("Pause"));DrawPerformance();
    if(M->Phase==TEXT("SHOP")&&!C->bShopOpen){U.Text(TEXT("F8 键盘 / 鼠标双人"),22,80,13,Gold);U.Text(TEXT("当前：")+C->ControlLabel(),22,133,11,Gold);U.Hit(18,75,220,25,TEXT("KeyboardMouse"));U.Text(TEXT("F2 双手柄   ·   F3 键鼠 + 手柄"),22,108,11,Muted);U.Hit(18,104,93,24,TEXT("TwoPads"));U.Hit(118,104,145,24,TEXT("Mixed"));}
    if(!C->bShopOpen)
    {
        float RY=108;
        U.Panel(W-215,95,195,32+Roster.Num()*26,10,Alpha(Ink,.76f));U.Text(TEXT("参战术士"),W-199,101,10,Gold);RY=124;
        Roster.Sort([](const AWarlockPawn& A,const AWarlockPawn& B){return A.State()&&B.State()?A.State()->Points>B.State()->Points:false;});
        for(auto Other:Roster)if(auto OS=Other->State())
        {float X=W-190;auto Col=Other==P?Gold:Muted;U.Arc(X,RY+7,3,0,360,Other->bAlive?Accent(OS->Team%8):Alpha(Muted,.4f),3);U.Text(OS->GetPlayerName(),X+14,RY,13,Col);U.Text(M->Phase==TEXT("SHOP")?(OS->bReady?TEXT("就绪"):TEXT("等待")):FString::FromInt(OS->Points),W-31,RY,12,OS->bReady&&M->Phase==TEXT("SHOP")?FLinearColor(.55f,.8f,.6f):Col,true);RY+=26;}
        if(Compact)
        {
            float BS=FMath::Clamp(Canvas->SizeY/900.f,.8f,1.55f),BW=Canvas->SizeX/BS,BH=Canvas->SizeY/BS;
            Painter B{this,Canvas,InterfaceFont,BS};
            B.Panel(12,BH-75,166,64,9,Alpha(Ink,.75f));B.Text(S->GetPlayerName()+TEXT(" · ")+C->ControlLabel(),20,BH-68,11,Gold);
            B.Text(FString::Printf(TEXT("%.1f"),FMath::Max(0.f,P->Health)),20,BH-52,21,Ivory);B.Text(FString::Printf(TEXT("/%.0f"),P->MaxHealth()),81,BH-40,10,Muted);
            B.Text(FString::Printf(TEXT("K %.1f"),P->Knockback),120,BH-43,12,P->Knockback>=70?FLinearColor(.95f,.29f,.17f):P->Knockback>=30?FLinearColor(.92f,.54f,.22f):Gold);
            float HP=FMath::Clamp(P->Health/P->MaxHealth(),0.f,1.f);B.Round(20,BH-22,150,4,2,Alpha(Muted,.25f));B.Round(20,BH-22,150*HealthDisplays.FindChecked(P).Trail,4,2,Alpha(Gold,.6f));B.Round(20,BH-22,150*HP,4,2,HP<.25f?FLinearColor(.8f,.28f,.18f):FLinearColor(.48f,.73f,.6f));
            float Start=BW/2-212,Y=BH-45;B.Panel(Start-4,BH-76,432,68,11,Alpha(Ink,.76f));
            const TCHAR* KeyboardKeys[]={TEXT("G"),TEXT("F"),TEXT("D"),TEXT("R"),TEXT("T"),TEXT("E"),TEXT("C"),TEXT("Y")};
            const TCHAR* PadKeys[]={TEXT("RT"),TEXT("LT"),TEXT("X"),TEXT("A"),TEXT("Y"),TEXT("B"),TEXT("LB"),TEXT("RB")};
            for(int I=0;I<8;++I)
            {
                bool Second=I==3&&P->SecondBlink>0;
                float X=Start+23+I*54;int L=S->Level(I);float CD=Second?0:P->Cooldowns[I];auto Color=Accent(I);
                B.Round(X-20,Y-20,40,40,20,FLinearColor(.012f,.019f,.023f,.8f));B.Arc(X,Y,20,-145,290,Alpha(Gold,L?.48f:.12f),1);
                B.Icon(I==6&&S->DefenseChoice==1?20:I,X,Y,21,L?Alpha(Color,CD>0?.28f:1):Alpha(Muted,.25f));
                if(Second){B.Arc(X,Y,22,-90,360*FMath::Clamp(P->SecondBlink/3.3f,0.f,1.f),Gold,2);B.Text(TEXT("II"),X,Y-8,15,Ivory,true);}
                if(CD>0){float Total=I==6&&S->DefenseChoice==1?22.f-L:WarlockRules::Cooldown(I,FMath::Max(1,L),S->Modes[I]);B.Arc(X,Y,20,-90,360*FMath::Clamp(CD/Total,0.f,1.f),Color,1.8f);B.Text(CD>=10?FString::Printf(TEXT("%.0f"),CD):FString::Printf(TEXT("%.1f"),CD),X,Y-8,15,Ivory,true);}
                const TCHAR* Key=C->LocalControl==2?(C->SelectedSpell==I?TEXT("左键"):TEXT("滚轮")):(C->LocalControl==3||(C->LocalControl==0&&C->bGamepadAiming))?PadKeys[I]:KeyboardKeys[I];B.Text(Key,X,Y+23,11,L?Muted:Alpha(Muted,.45f),true);
                if(L)B.Text(FString::FromInt(L),X+16,Y+9,9,Gold,true);
                if(L&&S->Modes[I]&&(I==3||I==4))B.Text(I==3?TEXT("折"):TEXT("齐"),X-15,Y-24,9,Gold,true);
                if(C->LocalControl==2){B.Hit(X-25,Y-23,50,60,FName(*FString::Printf(TEXT("Spell%d"),I)));if(C->SelectedSpell==I)B.Arc(X,Y,23,0,360,Gold,1.3f);}
            }
            if(C->LocalControl==2)B.Text(C->SelectedSpell==6&&S->DefenseChoice==1?TEXT("急行"):Names[FMath::Clamp(C->SelectedSpell,0,7)],BW/2,BH-94,11,Gold,true);
            B.Panel(BW-130,BH-68,118,56,9,Alpha(Ink,.72f));B.Icon(15,BW-111,BH-43,12,Gold);B.Text(FString::Printf(TEXT("%d 金"),S->Gold),BW-91,BH-58,19,Gold);
            if(M->Phase==TEXT("FINAL")){B.Text(TEXT("再来一场"),BW-72,BH-31,11,Ivory,true);B.Hit(BW-125,BH-34,108,24,TEXT("Replay"));}
            else if(!P->bAlive)B.Text(TEXT("等待复活"),BW-71,BH-30,10,Muted,true);
        }
        else
        {
        U.Panel(18,H-145,218,135,13,Alpha(Ink,.9f));U.Text(S->GetPlayerName(),30,H-131,13,Gold);
        const TCHAR* LookNames[]={TEXT("赤曜术士"),TEXT("霜月术士"),TEXT("幽林术士")};U.Text(LookNames[FMath::Clamp(S->Appearance,0,2)],191,H-130,10,Muted,true);
        U.Text(FString::Printf(TEXT("%.1f"),FMath::Max(0.f,P->Health)),30,H-108,36,Ivory);U.Text(FString::Printf(TEXT("/ %.0f"),P->MaxHealth()),143,H-87,16,Muted);
        FLinearColor Vital=P->Health<P->MaxHealth()*.25f?FLinearColor(.8f,.28f,.18f):FLinearColor(.48f,.73f,.6f);
        U.Round(30,H-52,192,7,3,Alpha(Muted,.22f));U.Round(30,H-52,192*HealthDisplays.FindChecked(P).Trail,7,3,Alpha(Gold,.65f));U.Round(30,H-52,192*FMath::Clamp(P->Health/P->MaxHealth(),0.f,1.f),7,3,Vital);
        for(int I=1;I<10;++I)U.Line(30+I*19.2f,H-52,30+I*19.2f,H-45,Alpha(Ink,.75f));
        U.Text(TEXT("击退累积"),30,H-33,11,Muted);U.Text(FString::Printf(TEXT("%.1f"),P->Knockback),103,H-36,17,Gold);
        float Start=W/2-304,Y=H-66;
        U.Panel(Start-7,Y-74,622,130,16,Alpha(Ink,.92f));U.Divider(Start+212,Y-64,184,Alpha(Gold,.6f));
        const TCHAR* Keys[]={TEXT("G · RT"),TEXT("F · LT"),TEXT("D · X"),TEXT("R · A"),TEXT("T · Y"),TEXT("E · B"),TEXT("C · LB"),TEXT("Y · RB")};
        for(int I=0;I<8;++I)
        {
            float X=Start+38+I*76;int L=S->Level(I);float CD=P->Cooldowns[I];auto Color=Accent(I==20?6:I);
            U.Round(X-31,Y-31,62,62,31,FLinearColor(.08f,.075f,.06f,.8f));U.Round(X-29,Y-29,58,58,29,FLinearColor(.016f,.023f,.026f,.96f));
            U.Arc(X,Y,31,-150,115,Alpha(Gold,L?.42f:.14f),1);U.Arc(X,Y,31,30,115,Alpha(Gold,L?.42f:.14f),1);
            U.Icon(I==6&&S->DefenseChoice==1?20:I,X,Y,30,L?Alpha(Color,CD>0?.25f:1.f):Alpha(Muted,.25f));
            if(CD>0){float Total=I==6&&S->DefenseChoice==1?22.f-L:WarlockRules::Cooldown(I,FMath::Max(1,L),S->Modes[I]);U.Arc(X,Y,29,-90,360*FMath::Clamp(CD/Total,0.f,1.f),Alpha(Color,.9f),2.3f);U.Text(CD>=10?FString::Printf(TEXT("%.0f"),CD):FString::Printf(TEXT("%.1f"),CD),X,Y-11,21,Ivory,true);}
            else if(L){U.Arc(X,Y,29,62,56,Alpha(Color,.8f),1.7f);U.Arc(X,Y-31,1,0,360,Alpha(Color,.8f),2);}
            U.Text((I==6&&S->DefenseChoice==1?TEXT("急行"):Names[I]),X,Y-53,14,L?Ivory:Muted,true);U.Text(C->LocalControl==2?(C->SelectedSpell==I?TEXT("左键释放"):TEXT("滚轮选择")):Keys[I],X,Y+38,12,L?Muted:Alpha(Muted,.5f),true);
            if(C->LocalControl==2){U.Hit(X-33,Y-57,66,91,FName(*FString::Printf(TEXT("Spell%d"),I)));if(C->SelectedSpell==I)U.Arc(X,Y,34,0,360,Gold,2);}
            if(L){U.Round(X+17,Y+16,17,17,8,FLinearColor(.025f,.037f,.045f));U.Text(FString::FromInt(L),X+25,Y+18,10,Gold,true);}
        }
        U.Panel(W-204,H-133,185,123,13,Alpha(Ink,.9f));U.Text(TEXT("回合补给"),W-187,H-120,10,Muted);U.Icon(15,W-158,H-84,22,Gold);U.Text(FString::FromInt(S->Gold),W-132,H-101,29,Gold);U.Text(TEXT("金币"),W-132,H-69,11,Muted);
        if(M->Phase==TEXT("SHOP")){U.Round(W-185,H-43,158,31,8,Alpha(Gold,.13f));U.Text(C->LocalControl==2?TEXT("点击打开商店"):TEXT("Tab / View  商店"),W-106,H-35,13,Gold,true);U.Hit(W-185,H-43,158,31,TEXT("Shop"));}
        else if(M->Phase==TEXT("FINAL")){U.Round(W-185,H-43,158,31,8,Alpha(Gold,.18f));U.Text(TEXT("再来一场"),W-106,H-35,13,Gold,true);U.Hit(W-185,H-43,158,31,TEXT("Replay"));}
        else U.Text(P->bAlive?TEXT("预判 · 走位 · 击退"):TEXT("等待下一回合复活"),W-106,H-28,12,Muted,true);
        }
        if(M->Phase==TEXT("FIGHT")&&P->bAlive)
        {FVector2D Aim;if(C->ProjectWorldLocationToScreen(C->LocalAim,Aim,true)){float X=Aim.X/Scale,Y2=Aim.Y/Scale;U.Arc(X,Y2,8,35,110,Alpha(Ivory,.8f));U.Arc(X,Y2,8,215,110,Alpha(Ivory,.8f));U.Line(X-14,Y2,X-10,Y2,Gold);U.Line(X+10,Y2,X+14,Y2,Gold);U.Arc(X,Y2,1,0,360,Gold,2);}}
    }
    else
    {
        float PW=FMath::Min(1120.f,W-48),PH=580,X=(W-PW)/2,Top=FMath::Max(107.f,(H-PH)/2),Main=PW*.665f,DX=X+Main+22,DW=PW-Main-44;
        U.Rect(0,98,W,H-98,FLinearColor(.004f,.008f,.012f,.68f));U.Panel(X,Top,PW,PH,16,Ink);U.Divider(X+30,Top+81,Main-54);
        U.Text(TEXT("回 合 商 店"),X+24,Top+21,25,Ivory);const TCHAR* Looks[]={TEXT("赤曜"),TEXT("霜月"),TEXT("幽林")};U.Text(FString::Printf(TEXT("%s外观 · H / 左摇杆 切换 · 外观不影响属性"),Looks[FMath::Clamp(S->Appearance,0,2)]),X+24,Top+57,12,Muted);U.Icon(15,DX+15,Top+36,22,Gold);U.Text(FString::Printf(TEXT("%d  金币"),S->Gold),DX+36,Top+24,23,Gold);
        U.Line(X+PW-35,Top+23,X+PW-24,Top+34,Muted);U.Line(X+PW-24,Top+23,X+PW-35,Top+34,Muted);U.Hit(X+PW-49,Top+9,43,43,TEXT("ShopClose"));U.Line(X+Main+2,Top+94,X+Main+2,Top+PH-25,Alpha(Muted,.17f));
        bool Equipment=C->SelectedProduct>=12&&C->SelectedProduct<20;U.Text(TEXT("法术与精通"),X+24,Top+93,16,Equipment?Muted:Gold);U.Text(TEXT("战斗装备"),X+160,Top+93,16,Equipment?Gold:Muted);U.Rect(X+(Equipment?160:24),Top+121,Equipment?64:82,2,Gold);
        U.Hit(X+16,Top+84,126,40,TEXT("Spells"));U.Hit(X+149,Top+84,125,40,TEXT("Equipment"));U.Text(TEXT("LB / RB 切换"),X+Main-124,Top+98,10,Muted);
        float CW=(Main-68)/4,CH=105;
        for(int N=0;N<8;++N)
        {
            int I=Equipment?N+12:((N==6&&(C->SelectedProduct==20||S->DefenseChoice==1))?20:N);float CX=X+24+(N%4)*(CW+7),CY=Top+139+(N/4)*(CH+10);bool Selected=C->SelectedProduct==I;auto Color=Accent(I==20?6:I);
            int Price=GetDefault<AWarlockMode>()->Price(P,I),Level=I==20?(S->DefenseChoice==1?S->Level(6):0):I<8?S->Level(I):S->Items[I-12];
            if(Selected){U.Round(CX-2,CY-3,CW+4,CH+8,11,FLinearColor(.075f,.066f,.043f,.7f));U.Line(CX+13,CY+CH+4,CX+CW-13,CY+CH+4,Alpha(Gold,.65f),1.5f);}
            U.Round(CX+CW/2-25,CY+4,50,50,25,Selected?Alpha(Color,.12f):FLinearColor(.009f,.016f,.023f,.7f));
            U.Arc(CX+CW/2,CY+29,25,0,360,Alpha(Color,Selected?.8f:.2f),Selected?1.6f:1.f);
            U.Icon(I,CX+CW/2,CY+29,31,Alpha(Color,Price<0&&Level==0?.35f:1));U.Text((I==6&&S->DefenseChoice==1?TEXT("急行"):Names[I]),CX+CW/2,CY+59,16,Selected?Ivory:Alpha(Ivory,.8f),true);
            FString Sub=I==1?TEXT("初始技能"):Price<0?(Level?TEXT("上限 / 前置不足"):TEXT("暂不可购买")):Level?FString::Printf(TEXT("等级 %d  ·  %d 金币"),Level,Price):FString::Printf(TEXT("购买  ·  %d 金币"),Price);
            U.Text(Sub,CX+CW/2,CY+84,11,Selected?Gold:Muted,true);if(Selected)U.Round(CX+CW/2-24,CY+CH-2,48,2,1,Gold);U.Hit(CX,CY,CW,CH,FName(*FString::Printf(TEXT("Select%d"),I)));
        }
        if(!Equipment)
        {
            U.Text(TEXT("永久精通"),X+24,Top+377,11,Muted);
            for(int I=8;I<12;++I){float CX=X+24+(I-8)*(CW+7),CY=Top+399;bool Selected=C->SelectedProduct==I;if(Selected)U.Round(CX,CY,CW,57,8,FLinearColor(.035f,.048f,.05f));U.Icon(I,CX+23,CY+27,23,Accent(I));U.Text(Names[I],CX+43,CY+10,13,Selected?Gold:Ivory);int L=I==8?S->LifeMastery:I==9?S->RangeMastery:I==10?S->TimeMastery:0;U.Text(I==11?TEXT("三项各升一级"):FString::Printf(TEXT("%d / 6"),L),CX+43,CY+32,11,Muted);U.Hit(CX,CY,CW,57,FName(*FString::Printf(TEXT("Select%d"),I)));}
        }
        else {int Slots=0;for(int L:S->Items)Slots+=L>0;U.Text(FString::Printf(TEXT("装备位置  %d / 2     ·     同类升级不额外占位置"),Slots),X+24,Top+393,13,Gold);U.Text(TEXT("出售整件装备，按当前等级返还金币；乔丹之石不可出售。"),X+24,Top+427,12,Muted);}
        const auto Shop=GetDefault<AWarlockMode>();
        int Selected=FMath::Clamp(C->SelectedProduct,0,20),Slot=Selected==20?6:Selected;
        int Price=Shop->NextCost(P,Selected);FString Why=Shop->PurchaseReason(P,Selected);bool Can=Why.IsEmpty();
        U.Icon(Selected,DX+31,Top+137,37,Accent(Slot));U.Text(Names[Selected],DX+62,Top+123,23,Ivory);
        U.Text(Selected<8||Selected==20?TEXT("法术构筑 / 原图等级与价格"):Selected<12?TEXT("永久研究 / 本场生效"):TEXT("战斗装备 / 两格容量"),DX+62,Top+158,11,Gold);
        U.Divider(DX+10,Top+189,DW-20);
        if((Selected==3||Selected==4)&&S->Level(Selected)>0)
        {FString Label=Selected==3?(S->Modes[3]?TEXT("折跃 → 闪现"):TEXT("闪现 → 折跃")):(S->Modes[4]?TEXT("齐射 → 连珠"):TEXT("连珠 → 齐射"));U.Round(DX+8,Top+202,DW-16,35,7,FLinearColor(.03f,.055f,.055f));U.Text(Label+TEXT(" · 免费  M / X"),DX+DW/2,Top+212,13,Gold,true);U.Hit(DX+8,Top+202,DW-16,35,TEXT("SkillMode"));}
        else if(Selected==6||Selected==20)
        {U.Round(DX+8,Top+202,DW-16,35,7,FLinearColor(.03f,.055f,.055f));U.Text(Selected==6?TEXT("查看急行 · Q / R3"):TEXT("查看反射盾 · Q / R3"),DX+DW/2,Top+212,13,Gold,true);U.Hit(DX+8,Top+202,DW-16,35,TEXT("Alternative"));}
        else U.Text(Selected==15?TEXT("一次购买 → 选择技能 → 启用"):TEXT("本级与下一级属性"),DX+10,Top+213,13,Gold);
        U.Wrapped(Shop->ProductDetails(P,Selected),DX+10,Top+253,DW-20,13,Alpha(Ivory,.9f));
        bool LearnedFamily=Slot!=6||S->DefenseChoice==(Selected==20?1:0);
        if(S->bHasStone&&S->StoneSlot<0&&Slot>=0&&Slot<8&&Slot!=1&&S->Level(Slot)&&LearnedFamily)
        {U.Round(DX+8,Top+432,DW-16,31,6,FLinearColor(.065f,.05f,.027f));U.Text(TEXT("启用乔丹之石于此槽 · J / LT"),DX+DW/2,Top+440,12,Gold,true);U.Hit(DX+8,Top+432,DW-16,31,TEXT("AssignStone"));}
        int Refund=Shop->SalePrice(P,Selected);
        if(Refund>=0){U.Round(DX+8,Top+432,DW-16,31,6,FLinearColor(.065f,.035f,.027f));U.Text(FString::Printf(TEXT("整件出售 +%d 金币 · Del / RT"),Refund),DX+DW/2,Top+440,12,Gold,true);U.Hit(DX+8,Top+432,DW-16,31,TEXT("SellSelected"));}
        U.Wrapped(Can?FString::Printf(TEXT("本次 %d 金币 · 购买后剩余 %d"),Price,S->Gold-Price):Price>=0?FString::Printf(TEXT("本次价格 %d · %s"),Price,*Why):Why,DX+10,Top+476,DW-20,12,Can?Gold:Muted);
        U.Panel(DX+7,Top+516,DW-14,43,9,Can?FLinearColor(.20f,.14f,.066f):FLinearColor(.04f,.05f,.053f));U.Text(Can?FString::Printf(TEXT("%s · %d 金币"),(Selected<8||Selected==20)&&!S->Level(Slot)?TEXT("学习"):TEXT("购买 / 升级"),Price):TEXT("条件未满足"),DX+DW/2,Top+527,17,Can?Gold:Muted,true);U.Hit(DX+7,Top+516,DW-14,43,TEXT("BuySelected"));
        if(!Equipment)
        {U.Text(TEXT("C 槽路线"),X+24,Top+473,11,Muted);for(int K=0;K<2;++K){int ID=K?20:6;float CX=X+24+K*(Main-55)/2;U.Round(CX,Top+495,(Main-65)/2,31,7,C->SelectedProduct==ID?FLinearColor(.065f,.06f,.04f):FLinearColor(.025f,.035f,.04f));U.Text(K?TEXT("急行 · 机动与伤害吸收"):TEXT("反射盾 · 反弹飞弹"),CX+(Main-65)/4,Top+503,12,C->SelectedProduct==ID?Gold:Muted,true);U.Hit(CX,Top+495,(Main-65)/2,31,FName(*FString::Printf(TEXT("Select%d"),ID)));}}
        U.Text(TEXT("初始 20 金 · 每回合 +10 · 余额保留 · 无利息"),X+24,Top+551,11,Muted);
        U.Text(TEXT("A / Enter 购买 · B / Tab 返回 · Y / 空格 准备"),X+24,Top+532,11,Gold);

    }
    if(C->NoticeTime>0)U.Text(C->Notice,W/2,C->bShopOpen?H-24:104,13,Gold,true);
    else if(!P->bAlive&&M->Phase==TEXT("FIGHT")){auto Target=P->Spectating.Get();U.Text(Target&&Target->State()?FString::Printf(TEXT("观战：%s · 等待下回合复活"),*Target->State()->GetPlayerName()):TEXT("等待回合结算"),W/2,107,14,Gold,true);}
    else if(M->Phase==TEXT("FIGHT")&&P->PendingCast>=0){U.Text(FString::Printf(TEXT("%s · %.2f 秒  /  S、中键、R3取消"),(P->PendingCast==6&&S->DefenseChoice==1?TEXT("急行"):Names[P->PendingCast]),P->CastRemaining),W/2,107,14,Gold,true);U.Round(W/2-90,130,180,3,1,Alpha(Muted,.3f));U.Round(W/2-90,130,180*(1-P->CastRemaining/FMath::Max(.001f,P->CastDuration)),3,1,Gold);}
    else if(M->Phase==TEXT("FIGHT")&&P->Protection>0)U.Text(TEXT("开场保护"),W/2,107,14,Accent(6),true);
    else if(M->Phase==TEXT("FIGHT")&&P->GetActorLocation().Size2D()>M->ArenaRadius)U.Text(TEXT("身处岩浆  ·  立即返回安全区域"),W/2,107,15,Accent(0),true);
    else if(M->Phase==TEXT("FINAL")){U.Text(M->Announcement,W/2,H-191,24,Gold,true);U.Text(TEXT("Enter / A 再来一场"),W/2,H-155,15,Ivory,true);}
    else if(M->Phase==TEXT("RESULT"))U.Text(M->Announcement,W/2,H-169,20,Gold,true);
    if(C->bHelpOpen)
    {
        float X=W/2-285,Y=H/2-174;U.Rect(0,0,W,H,FLinearColor(0,0,0,.55f));U.Round(X,Y,570,348,16,Ink);U.Text(TEXT("操作指南"),X+26,Y+22,26,Gold);U.Text(TEXT("键鼠"),X+28,Y+79,17,Ivory);U.Text(TEXT("右键点击移动 / 方向键移动\n鼠标瞄准，左键发射火球\nG F D R T E C Y 对应八个法术\nTab 商店，Enter 确认；空格准备"),X+28,Y+114,14,Muted);U.Text(TEXT("Xbox 手柄"),X+305,Y+79,17,Ivory);U.Text(TEXT("左摇杆移动并决定施法方向\nRT / LT / X / A / Y / B / LB / RB\nView 打开商店，十字键选择\nA 确认，B 返回，商店Y准备\n练习时↑准备，LB/RB切页"),X+305,Y+114,14,Muted);U.Line(X+28,Y+245,X+542,Y+245,Alpha(Muted,.2f));U.Text(TEXT("F2 双手柄加入      F3 键鼠 + 手柄\nF4 自由竞技 / 合作（重开）      F5 分屏布局\nF8 键盘/鼠标分开双人；S/中键/R3取消。"),X+28,Y+265,13,Gold);U.Hit(X,Y,570,348,TEXT("Help"));
    }
}
void AWarlockHUD::NotifyHitBoxClick(FName Name)
{
    Super::NotifyHitBoxClick(Name);auto C=Cast<AWarlockController>(PlayerOwner);if(!C)return;
    if(Name==TEXT("Resume")){C->TogglePause();return;}
    if(UGameplayStatics::IsGamePaused(this)&&Name==TEXT("DisplayTab")){C->ToggleDisplaySettings();return;}
    if(UGameplayStatics::IsGamePaused(this)&&Name.ToString().StartsWith(TEXT("Display")))
    {FString Row,Direction;if(Name.ToString().Mid(7).Split(TEXT("_"),&Row,&Direction))C->EditDisplaySetting(FCString::Atoi(*Row),FCString::Atoi(*Direction));return;}
    if(Name.ToString().StartsWith(TEXT("Difficulty"))){C->SetAIDifficulty(FCString::Atoi(*Name.ToString().Mid(10)));return;}
    if(UGameplayStatics::IsGamePaused(this))return;
    if(Name==TEXT("Pause")){C->TogglePause();return;}
    if(Name==TEXT("Ready")){C->ToggleReady();return;}
    if(Name==TEXT("KeyboardMouse")){C->AddKeyboardMousePlayer();return;}if(Name==TEXT("TwoPads")){C->AddLocalPlayer();return;}if(Name==TEXT("Mixed")){C->AddMixedPlayer();return;}
    if(C->LocalControl==2&&Name.ToString().StartsWith(TEXT("Spell"))){C->SelectedSpell=FMath::Clamp(FCString::Atoi(*Name.ToString().Mid(5)),0,7);return;}
    if(Name==TEXT("Help")){C->ToggleHelp();return;}if(Name==TEXT("Shop")){C->ToggleShop();return;}
    if(Name==TEXT("Replay")){if(auto M=GetWorld()->GetGameState<AWarlockMatchState>();M&&M->Phase==TEXT("FINAL"))C->ConfirmShop();return;}if(!C->bShopOpen)return;
    if(Name==TEXT("ShopClose")){C->bShopOpen=false;return;}if(Name==TEXT("BuySelected")){C->ServerBuy(C->SelectedProduct);return;}
    if(Name==TEXT("Spells")){C->SelectedProduct=0;return;}if(Name==TEXT("Equipment")){C->SelectedProduct=12;return;}
    if(Name==TEXT("SkillMode")){C->ToggleMode();return;}if(Name==TEXT("Alternative")){C->AlternativeSelected();return;}
    if(Name==TEXT("SellSelected")){C->SellSelected();return;}if(Name==TEXT("AssignStone")){C->StoneSelected();return;}
    FString Value=Name.ToString();if(Value.StartsWith(TEXT("Select")))C->SelectedProduct=FMath::Clamp(FCString::Atoi(*Value.Mid(6)),0,20);
}
