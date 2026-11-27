#include "WarlockGame.h"
#include "WarlockPainter.h"
#include "WarlockAppearance.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"

void AWarlockHUD::DrawRoomLobby()
{
    auto C=Cast<AWarlockController>(PlayerOwner);auto M=GetWorld()->GetGameState<AWarlockMatchState>();auto S=C?C->GetPlayerState<AWarlockState>():nullptr;
    if(!C||!M||!S)return;using namespace WarlockUI;
    float Scale=FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/760.f),W=Canvas->SizeX/Scale,H=Canvas->SizeY/Scale;
    Painter U{this,Canvas,InterfaceFont,Scale};U.Rect(0,0,W,H,FLinearColor(.006f,.012f,.02f,.86f));
    float X=(W-820)/2,Y=(H-610)/2;
    U.Panel(X,Y,820,610,18,Ink);U.Text(TEXT("余 烬 集 会"),W/2,Y+26,30,Ivory,true);
    U.Text(TEXT("专用服务器大厅 · 至少两位玩家 · AI 自动补位"),W/2,Y+72,13,Muted,true);
    bool Host=S->GetPlayerId()==M->RoomHostId;
    U.Text(FString::Printf(TEXT("%d 席  ·  AI 难度 %d / 6  ·  %s"),M->ParticipantCount,M->AIDifficulty,M->bTeams?TEXT("玩家合作对抗 AI"):TEXT("自由竞技")),W/2,Y+105,16,Gold,true);
    int N=0,Humans=0,Ready=0;
    for(TActorIterator<AWarlockPawn> It(GetWorld());It;++It)
    {
        auto P=*It;auto PS=P->State();if(!PS)continue;
        if(!PS->bBot){++Humans;Ready+=PS->bReady;}
        float RY=Y+151+N++*35;U.Round(X+35,RY,750,29,6,FLinearColor(.025f,.04f,.05f,.8f));
        U.Arc(X+56,RY+14,5,0,360,WarlockVisual::SeatColor(PS->ColorIndex),4);
        U.Text(PS->GetPlayerName()+(PS->GetPlayerId()==M->RoomHostId?TEXT(" · 房主"):TEXT("")),X+78,RY+6,14,Ivory);
        U.Text(PS->bBot?TEXT("AI · 已准备"):PS->bReady?TEXT("已准备"):TEXT("等待准备"),X+727,RY+6,13,PS->bReady?FLinearColor(.4f,.8f,.6f):Muted,true);
    }
    auto Button=[&](float BX,float BY,float BW,const TCHAR* Label,const TCHAR* ID){U.Round(BX,BY,BW,34,7,Alpha(Gold,.12f));U.Text(Label,BX+BW/2,BY+9,13,Gold,true);U.Hit(BX,BY,BW,34,FName(ID));};
    if(Host){Button(X+35,Y+446,65,TEXT("席位 −"),TEXT("RoomSeatsLess"));Button(X+108,Y+446,65,TEXT("席位 +"),TEXT("RoomSeatsMore"));Button(X+187,Y+446,130,TEXT("切换 AI 难度"),TEXT("RoomDifficulty"));Button(X+331,Y+446,150,TEXT("切换竞技 / 合作"),TEXT("RoomTeams"));}
    U.Text(Host?TEXT("Enter / 手柄 A 准备 · 十字键席位 · RB 难度 · Y 模式"):TEXT("房间规则由房主调整 · Enter / 手柄 A 准备"),X+35,Y+492,12,Muted);
    Button(X+35,Y+538,470,*FString::Printf(TEXT("%s · %d / %d 玩家准备"),S->bReady?TEXT("取消准备"):TEXT("准备完毕"),Ready,Humans),TEXT("RoomReady"));
    Button(X+527,Y+538,258,TEXT("离开房间，返回单机"),TEXT("LeaveRoom"));
    if(!C->Notice.IsEmpty()&&C->NoticeTime>0)U.Text(C->Notice,W/2,Y+585,12,Gold,true);
}
