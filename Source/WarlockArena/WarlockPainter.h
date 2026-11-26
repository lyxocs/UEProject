#pragma once
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "GameFramework/HUD.h"
#include "CanvasItem.h"
#include "GlobalRenderResources.h"
#include "Fonts/SlateFontInfo.h"

namespace WarlockUI
{
inline const FLinearColor Gold(.79f,.59f,.31f,1),Ivory(.91f,.9f,.84f,1),Muted(.43f,.5f,.53f,1),Ink(.008f,.013f,.021f,.94f);
inline FLinearColor Alpha(FLinearColor C,float A){C.A=A;return C;}
inline FLinearColor Accent(int I)
{
    const FLinearColor Colors[]={FLinearColor(.95f,.38f,.15f),FLinearColor(.88f,.72f,.36f),FLinearColor(.43f,.76f,.93f),FLinearColor(.53f,.52f,.9f),FLinearColor(.91f,.53f,.23f),FLinearColor(.87f,.29f,.17f),FLinearColor(.35f,.75f,.72f),FLinearColor(.39f,.65f,.83f)};
    return I<8?Colors[I]:I==8?FLinearColor(.57f,.77f,.49f):Gold;
}
// Resolution-independent authored vector UI; icons can be replaced without changing gameplay.
struct Painter
{
    AHUD* Hud;UCanvas* Canvas;UFont* Font;float S;
    void Rect(float X,float Y,float W,float H,FLinearColor C){Hud->DrawRect(C,X*S,Y*S,W*S,H*S);}
    void Line(float X,float Y,float EX,float EY,FLinearColor C,float Width=1){Hud->DrawLine(X*S,Y*S,EX*S,EY*S,C,Width*S);}
    void Poly(const TArray<FVector2D>& Points,FLinearColor Color)
    {
        if(Points.Num()<3)return;
        TArray<FCanvasUVTri> Tris;
        for(int I=1;I<Points.Num()-1;++I){FCanvasUVTri T;T.V0_Pos=Points[0]*S;T.V1_Pos=Points[I]*S;T.V2_Pos=Points[I+1]*S;T.V0_UV=T.V1_UV=T.V2_UV=FVector2D::ZeroVector;T.V0_Color=T.V1_Color=T.V2_Color=Color;Tris.Add(T);}
        FCanvasTriangleItem Item(Tris,GWhiteTexture);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);
    }
    void Arc(float X,float Y,float R,float Start,float Sweep,FLinearColor C,float Width=1)
    {
        int N=FMath::Max(2,FMath::CeilToInt(FMath::Abs(Sweep)/6));
        for(int I=0;I<N;++I){float A=FMath::DegreesToRadians(Start+Sweep*I/N),B=FMath::DegreesToRadians(Start+Sweep*(I+1)/N);Line(X+R*FMath::Cos(A),Y+R*FMath::Sin(A),X+R*FMath::Cos(B),Y+R*FMath::Sin(B),C,Width);}
    }
    void Round(float X,float Y,float W,float H,float R,FLinearColor C)
    {
        if(W<=0||H<=0)return;
        R=FMath::Min3(R,W/2,H/2);TArray<FVector2D> P;
        const FVector2D Centers[]={FVector2D(X+W-R,Y+R),FVector2D(X+W-R,Y+H-R),FVector2D(X+R,Y+H-R),FVector2D(X+R,Y+R)};
        for(int K=0;K<4;++K)for(int I=0;I<=6;++I){float A=FMath::DegreesToRadians(-90.f+90*K+90.f*I/6);P.Add(Centers[K]+FVector2D(FMath::Cos(A),FMath::Sin(A))*R);}
        Poly(P,C);
    }
    void Panel(float X,float Y,float W,float H,float R,FLinearColor Fill=Ink)
    {
        Round(X,Y+3,W,H,R,FLinearColor(0,0,0,.3f));
        Round(X,Y,W,H,R,FLinearColor(.07f,.085f,.10f,.48f));
        Round(X+1,Y+1,W-2,H-2,FMath::Max(0.f,R-1),Fill);
        Line(X+R+6,Y+1,X+W-R-6,Y+1,Alpha(Gold,.18f));
        Line(X+R,Y+1,X+R+14,Y+1,Alpha(Gold,.55f));Line(X+W-R-14,Y+1,X+W-R,Y+1,Alpha(Gold,.55f));
        Line(X+R,Y+H-1,X+R+10,Y+H-1,Alpha(Gold,.3f));Line(X+W-R-10,Y+H-1,X+W-R,Y+H-1,Alpha(Gold,.3f));
    }
    void Divider(float X,float Y,float W,FLinearColor Color=Gold)
    {
        Line(X,Y,X+W*.43f,Y,Alpha(Color,.3f));Line(X+W*.57f,Y,X+W,Y,Alpha(Color,.3f));
        Poly({FVector2D(X+W/2-3,Y),FVector2D(X+W/2,Y-3),FVector2D(X+W/2+3,Y),FVector2D(X+W/2,Y+3)},Alpha(Color,.7f));
    }
    void Text(const FString& V,float X,float Y,float Pixels,FLinearColor C=Ivory,bool Center=false)
    {
        FSlateFontInfo Info=Font->GetLegacySlateFontInfo();Info.Size=FMath::Max(7,FMath::RoundToInt(FMath::Max(Pixels*S,Pixels>=13?11.f:9.f)*.75f));
        FCanvasTextItem Item(FVector2D(FMath::RoundToFloat(X*S),FMath::RoundToFloat(Y*S)),FText::FromString(V),Info,C);Item.bCentreX=Center;Item.EnableShadow(FLinearColor(0,0,0,.35f),FVector2D(0,1));Canvas->DrawItem(Item);
    }
    void Wrapped(const FString& V,float X,float Y,float Width,float Pixels,FLinearColor Color)
    {
        const float FontScale=FMath::Max(Pixels*S,Pixels>=13?11.f:9.f)/FMath::Max(1.f,Font->GetMaxCharHeight());
        FString LineText;float Offset=0;
        for(TCHAR Ch:V)
        {
            FString Next=LineText;Next.AppendChar(Ch);float TW=0,TH=0;Canvas->StrLen(Font,Next,TW,TH);
            if(Ch=='\n'||(TW*FontScale>Width*S&&!LineText.IsEmpty())){Text(LineText,X,Y+Offset,Pixels,Color);Offset+=FMath::Max(Pixels+5,14.f/S);LineText.Empty();if(Ch=='\n')continue;}
            LineText.AppendChar(Ch);
        }
        if(!LineText.IsEmpty())Text(LineText,X,Y+Offset,Pixels,Color);
    }
    void Icon(int Index,float X,float Y,float Size,FLinearColor C)
    {
        auto L=[&](float A,float B,float D,float E,float Width=1.7f){Line(X+A*Size,Y+B*Size+1,X+D*Size,Y+E*Size+1,FLinearColor(0,0,0,C.A*.6f),Width+1);Line(X+A*Size,Y+B*Size,X+D*Size,Y+E*Size,C,Width);};
        auto P=[&](std::initializer_list<FVector2D> In){TArray<FVector2D> Out;for(auto V:In)Out.Add(FVector2D(X,Y)+V*Size);Poly(Out,C);};
        auto Ring=[&](float A,float B,float R){Arc(X+A*Size,Y+B*Size,R*Size,0,360,C,1.7f);};
        switch(Index)
        {
        case 0:P({{-.05f,-.49f},{.1f,-.18f},{.3f,-.3f},{.38f,.14f},{.2f,.41f},{-.13f,.46f},{-.38f,.18f},{-.31f,-.18f},{-.2f,-.02f}});Poly({FVector2D(X-.12f*Size,Y+.3f*Size),FVector2D(X-.19f*Size,Y+.04f*Size),FVector2D(X+.02f*Size,Y-.15f*Size),FVector2D(X+.15f*Size,Y+.18f*Size),FVector2D(X+.02f*Size,Y+.35f*Size)},Alpha(Ivory,C.A*.65f));break;
        case 1:Ring(0,0,.2f);for(int I=0;I<8;++I){float A=I*PI/4;L(FMath::Cos(A)*.31f,FMath::Sin(A)*.31f,FMath::Cos(A)*.47f,FMath::Sin(A)*.47f);}break;
        case 2:P({{.08f,-.5f},{-.35f,.08f},{-.02f,.08f},{-.12f,.5f},{.38f,-.14f},{.04f,-.14f}});break;
        case 3:L(-.47f,-.15f,-.15f,-.15f);L(-.55f,.08f,-.28f,.08f);L(-.38f,.3f,-.12f,.3f);L(-.08f,-.4f,.3f,0);L(.3f,0,-.08f,.4f);L(.12f,-.4f,.5f,0);L(.5f,0,.12f,.4f);break;
        case 4:for(int I=-1;I<=1;++I){L(-.35f,I*.25f,.05f,I*.34f);P({{.05f,I*.34f-.1f},{.4f,I*.34f},{.05f,I*.34f+.1f}});}break;
        case 5:Ring(.12f,.15f,.23f);L(-.42f,-.43f,.02f,0);L(-.12f,-.45f,.21f,-.11f);L(-.48f,-.15f,-.19f,.12f);break;
        case 6:L(-.37f,-.35f,0,-.48f);L(0,-.48f,.37f,-.35f);L(.37f,-.35f,.29f,.17f);L(.29f,.17f,0,.47f);L(0,.47f,-.29f,.17f);L(-.29f,.17f,-.37f,-.35f);L(-.16f,0,-.02f,.15f);L(-.02f,.15f,.2f,-.13f);break;
        case 7:Ring(-.18f,.13f,.24f);Ring(.18f,-.13f,.24f);L(-.13f,.13f,.16f,-.14f);break;
        case 8:P({{0,-.18f},{-.16f,-.34f},{-.35f,-.22f},{-.32f,.04f},{0,.38f},{.32f,.04f},{.35f,-.22f},{.16f,-.34f}});break;
        case 9:Ring(0,0,.33f);L(-.5f,0,.5f,0);L(0,-.5f,0,.5f);Ring(0,0,.07f);break;
        case 10:L(-.29f,-.4f,.29f,-.4f);L(-.29f,.4f,.29f,.4f);L(-.24f,-.4f,-.16f,-.16f);L(-.16f,-.16f,.16f,.16f);L(.16f,.16f,.24f,.4f);L(.24f,-.4f,.16f,-.16f);L(.16f,-.16f,-.16f,.16f);L(-.16f,.16f,-.24f,.4f);break;
        case 11:for(int I=0;I<3;++I){float A=I*PI*2/3-PI/2;Ring(.26f*FMath::Cos(A),.26f*FMath::Sin(A),.18f);}break;
        case 12:L(-.15f,-.42f,.15f,-.42f);L(-.15f,-.42f,-.42f,.4f);L(.15f,-.42f,.42f,.4f);L(-.42f,.4f,-.08f,.28f);L(.42f,.4f,.08f,.28f);L(-.08f,.28f,0,-.13f);L(.08f,.28f,0,-.13f);break;
        case 13:case 16:P({{-.18f,-.4f},{.12f,-.4f},{.12f,.12f},{.38f,.24f},{.38f,.4f},{-.25f,.4f},{-.25f,.05f}});break;
        case 14:L(-.31f,.36f,.32f,-.4f,3);L(-.3f,.05f,.06f,.33f,2);L(-.43f,.47f,-.28f,.3f,3);break;
        case 15:case 18:L(0,-.46f,.35f,0);L(.35f,0,0,.46f);L(0,.46f,-.35f,0);L(-.35f,0,0,-.46f);L(-.35f,0,.35f,0);L(0,-.46f,.13f,0);L(.13f,0,0,.46f);break;
        case 17:Ring(0,.12f,.28f);L(-.15f,-.15f,0,-.4f);L(0,-.4f,.15f,-.15f);break;
        case 20:L(-.48f,-.25f,.15f,-.25f);Arc(X+Size*.15f,Y-Size*.37f,Size*.12f,0,270,C,1.7f);L(-.5f,0,.33f,0);Arc(X+Size*.33f,Y+Size*.13f,Size*.13f,-90,250,C,1.7f);L(-.35f,.23f,.02f,.23f);break;
        case 19:Arc(X,Y,Size*.34f,180,180,C,2);L(-.34f,0,-.34f,.34f);L(.34f,0,.34f,.34f);L(-.34f,.34f,-.14f,.27f);L(.34f,.34f,.14f,.27f);L(-.17f,.04f,.17f,.04f);L(0,-.12f,0,.38f);break;
        }
    }
    void Hit(float X,float Y,float W,float H,FName Name){Hud->AddHitBox(FVector2D(X*S,Y*S),FVector2D(W*S,H*S),Name,true);}
};
}
