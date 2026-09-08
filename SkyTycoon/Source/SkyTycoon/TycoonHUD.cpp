#include "TycoonGame.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"

namespace
{
    const FLinearColor SeatColors[] = { FLinearColor(1,.3f,.4f), FLinearColor(.2f,.65f,1), FLinearColor(1,.75f,.2f), FLinearColor(.65f,.4f,1) };
}
void ATycoonHUD::DrawHUD()
{
    Super::DrawHUD();
    const auto* G = GetWorld()->GetAuthGameMode<ATycoonGameMode>();
    if (!Canvas || !G || G->Match.Players.IsEmpty()) return;
    const auto& S = G->Match; const auto& P = S.Players[S.Turn];
    // One virtual layout is shared by drawing and hit testing, including letterboxing.
    UIScale = FMath::Min(Canvas->SizeX/1000.f,Canvas->SizeY/760.f);
    UIOrigin = FVector2D((Canvas->SizeX-1000*UIScale)/2,(Canvas->SizeY-760*UIScale)/2);
    const auto Rect = [&](FLinearColor C,float X,float Y,float W,float H)
    { DrawRect(C,UIOrigin.X+X*UIScale,UIOrigin.Y+Y*UIScale,W*UIScale,H*UIScale); };
    const auto Text = [&](const FString& T,float X,float Y,float Scale=1.f,FLinearColor C=FLinearColor::White)
    { DrawText(T,C,UIOrigin.X+X*UIScale,UIOrigin.Y+Y*UIScale,nullptr,Scale*UIScale); };
    const auto Button = [&](const FString& T,const FBox2D& Box,bool Enabled)
    {
        Rect(Enabled ? FLinearColor(.13f,.35f,.58f) : FLinearColor(.12f,.15f,.2f),Box.Min.X,Box.Min.Y,Box.GetSize().X,Box.GetSize().Y);
        Text(T,Box.Min.X+8,Box.Min.Y+10,.9f,Enabled ? FLinearColor::White : FLinearColor(.45f,.48f,.52f));
    };
    DrawRect(FLinearColor(.025f,.04f,.07f),0,0,Canvas->SizeX,Canvas->SizeY);
    Text(TEXT("SKY TYCOON  /  ITEMS & FACILITIES"),20,16,1.4f);
    SpeedBox=FBox2D(FVector2D(800,10),FVector2D(980,48));
    Button(G->bFastBots ? TEXT("AI SPEED: 2x") : TEXT("AI SPEED: 1x"),SpeedBox,true);
    TileBoxes.SetNum(S.Map.Tiles.Num());
    int32 Extent=1; for (const auto& T:S.Map.Tiles) Extent=FMath::Max(Extent,FMath::Max(T.Cell.X,T.Cell.Y)+1);
    const float Cell=500.f/Extent;
    for (int32 I=0;I<S.Map.Tiles.Num();++I)
    {
        const auto& T=S.Map.Tiles[I]; const float X=20+T.Cell.X*Cell,Y=62+T.Cell.Y*Cell;
        TileBoxes[I]=FBox2D(FVector2D(X,Y),FVector2D(X+Cell,Y+Cell));
        const int32 Seat=S.Owners[I];
        const bool Facility=T.Type>=ETycoonTile::Shop;
        Rect(I==G->SelectedTarget ? FLinearColor(.9f,.75f,.2f) : FLinearColor(.08f,.12f,.17f),X,Y,Cell,Cell);
        Rect(Seat==INDEX_NONE ? (Facility ? FLinearColor(.22f,.3f,.42f) : FLinearColor(.12f,.22f,.29f)) : SeatColors[Seat]*.55f,X+2,Y+2,Cell-4,Cell-4);
        Text(T.Name,X+5,Y+7,FMath::Clamp(Cell/80,.45f,.9f));
        if (T.Type==ETycoonTile::Property) Text(FString::Printf(TEXT("$%d"),T.Price),X+5,Y+Cell*.47f,.65f);
        if (S.Traps[I]!=ETycoonTrap::None) Text(S.Traps[I]==ETycoonTrap::Bomb ? TEXT("BOMB") : TEXT("STOP"),X+5,Y+Cell*.65f,.6f,FLinearColor(1,.65f,.1f));
        for (int32 J=0;J<S.Players.Num();++J) if (!S.Players[J].bBankrupt && S.Players[J].Position==I)
            Rect(SeatColors[J],X+5+J*12,Y+Cell-12,9,9);
    }
    Text(FString::Printf(TEXT("Round %d   Move %d"),S.Round,S.LastDie),140,190,1.4f);
    Text(P.Name+(P.bBot ? TEXT(" [AI]") : TEXT(" [YOU]")),140,225,1.4f,SeatColors[S.Turn]);
    Text(FString::Printf(TEXT("Car moves: %d"),P.CarRolls),140,263);
    Text(FString::Printf(TEXT("Shield: %s   Rent pass: %s"),P.bShield ? TEXT("ON") : TEXT("-"),P.bRentPass ? TEXT("ON") : TEXT("-")),140,288,.85f);
    if (P.Stay!=ETycoonStay::None)
    {
        const FString Stay=P.Stay==ETycoonStay::Jail ? TEXT("JAIL") : P.Stay==ETycoonStay::Hospital ? TEXT("HOSPITAL") : TEXT("INN");
        Text(FString::Printf(TEXT("%s: %d turn(s)"),*Stay,P.StayTurns),140,325,1.1f);
    }
    Text(FString::Printf(TEXT("DICE: %d / %d   click to toggle"),P.bBot ? P.MaxDice() : G->SelectedDiceCount(),P.MaxDice()),120,363,.9f);
    RollDiceBoxes.SetNum(3);
    for (int32 I=0;I<3;++I)
    {
        const bool Locked=I>=P.MaxDice();
        const bool Lit=!Locked && (P.bBot || (G->DiceMask&(1<<I))!=0);
        RollDiceBoxes[I]=FBox2D(FVector2D(130+I*90,397),FVector2D(202+I*90,450));
        Button(Locked ? TEXT("LOCK") : FString::Printf(TEXT("D%d %s"),I+1,Lit ? TEXT("ON") : TEXT("OFF")),RollDiceBoxes[I],Lit);
    }
    Text(FString::Printf(TEXT("INVENTORY %d/8 | click item to select"),P.InventorySize()),550,65);
    ItemBoxes.SetNum(static_cast<int32>(ETycoonItem::Count));
    for (int32 I=0;I<ItemBoxes.Num();++I)
    {
        const auto Item=static_cast<ETycoonItem>(I); const float Y=95+I*32;
        ItemBoxes[I]=FBox2D(FVector2D(550,Y),FVector2D(980,Y+28));
        Rect(Item==G->SelectedItem ? FLinearColor(.2f,.35f,.43f) : FLinearColor(.09f,.14f,.2f),550,Y,430,28);
        Text(FTycoonCatalog::Name(Item),558,Y+5,.9f);
        Text(FString::Printf(TEXT("$%d     x%d"),FTycoonCatalog::Price(Item),P.ItemCount(Item)),800,Y+5,.9f);
    }
    Text(TEXT("Trap: select a board tile (next 6, empty road)."),550,392,.85f);
    Text(FString::Printf(TEXT("Selected tile: %s"),S.Map.Tiles.IsValidIndex(G->SelectedTarget) ? *S.Map.Tiles[G->SelectedTarget].Name : TEXT("none")),550,413,.85f);
    DiceBoxes.SetNum(6);
    Text(TEXT("Remote dice:"),550,445,.9f);
    for (int32 I=0;I<6;++I)
    {
        DiceBoxes[I]=FBox2D(FVector2D(660+I*50,435),FVector2D(705+I*50,472));
        Button(FString::FromInt(I+1),DiceBoxes[I],G->SelectedDie==I+1);
    }
    // Short wrapped effect description prevents overflowing the panel.
    TArray<FString> Words; FTycoonCatalog::Help(G->SelectedItem).ParseIntoArray(Words,TEXT(" "));
    FString Line; float LineY=485;
    for (const auto& Word:Words)
    {
        if (Line.Len()+Word.Len()>56) { Text(Line,550,LineY,.8f); LineY+=18; Line.Reset(); }
        Line+=Word+TEXT(" ");
    }
    if (!Line.IsEmpty()) Text(Line,550,LineY,.8f);
    UseBox=FBox2D(FVector2D(550,530),FVector2D(980,567));
    const bool CanUse=!P.bBot && !S.bUsedItem && P.ItemCount(G->SelectedItem)>0 && (S.Phase==ETycoonPhase::Roll || S.Phase==ETycoonPhase::Detained);
    Button(S.bUsedItem ? TEXT("ITEM USED THIS TURN") : TEXT("USE SELECTED ITEM"),UseBox,CanUse);
    for (int32 I=0;I<S.Players.Num();++I)
        Text(FString::Printf(TEXT("%s: $%d%s"),*S.Players[I].Name,S.Players[I].Cash,S.Players[I].bBankrupt ? TEXT(" OUT") : TEXT("")),20+I*240,590,1.f,SeatColors[I]);
    Text(S.Message,20,623,.9f);
    FString Label=TEXT("END TURN [Space]");
    if (S.Phase==ETycoonPhase::Roll) Label=TEXT("ROLL [Space]");
    else if (S.Phase==ETycoonPhase::Purchase) Label=TEXT("BUY PROPERTY [Space]");
    else if (S.Phase==ETycoonPhase::Shopping) Label=S.bShopped ? TEXT("PURCHASE COMPLETE - LEAVE") : TEXT("BUY SELECTED ITEM [Space]");
    else if (S.Phase==ETycoonPhase::Lodging) Label=TEXT("CHECK IN $300 [Space]");
    else if (S.Phase==ETycoonPhase::Detained) Label=P.Stay==ETycoonStay::Jail ? TEXT("BAIL $800 [Space]") : P.Stay==ETycoonStay::Hospital ? TEXT("TREATMENT $600 [Space]") : TEXT("REST THIS TURN [Space]");
    else if (S.Phase==ETycoonPhase::Finished) Label=TEXT("FINISHED - press R to restart");
    if (P.bBot && S.Phase!=ETycoonPhase::Finished) Label=TEXT("AI THINKING...");
    PrimaryBox=FBox2D(FVector2D(20,670),FVector2D(630,721));
    SkipBox=FBox2D(FVector2D(650,670),FVector2D(980,721));
    Button(Label,PrimaryBox,!P.bBot && S.Phase!=ETycoonPhase::Finished);
    Button(S.Phase==ETycoonPhase::Detained ? TEXT("WAIT [E]") : TEXT("PASS / LEAVE [E]"),SkipBox,!P.bBot && S.Phase!=ETycoonPhase::Roll && S.Phase!=ETycoonPhase::Finished);
    Text(TEXT("One item per turn before rolling. R restarts the local match."),20,735,.75f);
}
void ATycoonHUD::ActivateAt(float X,float Y)
{
    auto* G=GetWorld()->GetAuthGameMode<ATycoonGameMode>();
    if (!G || UIScale<=0) return;
    const FVector2D Point=(FVector2D(X,Y)-UIOrigin)/UIScale;
    if (SpeedBox.IsInside(Point)) { G->ToggleBotSpeed(); return; }
    if (UseBox.IsInside(Point)) { G->UseSelected(); return; }
    if (PrimaryBox.IsInside(Point))
    {
        if (G->Match.Phase==ETycoonPhase::Shopping && G->Match.bShopped) G->Skip(); else G->Primary();
        return;
    }
    if (SkipBox.IsInside(Point)) { G->Skip(); return; }
    for (int32 I=0;I<ItemBoxes.Num();++I) if (ItemBoxes[I].IsInside(Point)) { G->SelectedItem=static_cast<ETycoonItem>(I); return; }
    for (int32 I=0;I<TileBoxes.Num();++I) if (TileBoxes[I].IsInside(Point)) { G->SelectedTarget=I; return; }
    for (int32 I=0;I<DiceBoxes.Num();++I) if (DiceBoxes[I].IsInside(Point)) { G->SelectedDie=I+1; return; }
    for (int32 I=0;I<RollDiceBoxes.Num();++I) if (RollDiceBoxes[I].IsInside(Point)) { G->ToggleDie(I); return; }
}
