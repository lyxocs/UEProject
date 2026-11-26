#include "WarlockGame.h"
#include "Engine/World.h"

namespace
{
constexpr int ItemCost[]={4,7,8,1,5,3,4,6};
constexpr int ItemCap[]={1,3,2,1,3,3,3,3};
constexpr int ItemRefund[]={3,5,7,0,4,2,3,5};
bool InShop(const AWarlockPawn* P)
{
    auto M=P?P->GetWorld()->GetGameState<AWarlockMatchState>():nullptr;
    return P&&P->State()&&M&&M->Phase==TEXT("SHOP");
}
int UsedSlots(const AWarlockState* S){int N=0;for(int L:S->Items)N+=L>0;return N;}
FString Gate(const AWarlockState* S,int Product)
{
    if(!S||Product<0||Product>20)return TEXT("无效商品");
    if(Product==6||Product==20)
    {
        int Choice=Product==20?1:0;
        if(S->Level(6)>0&&S->DefenseChoice!=Choice)return TEXT("C 槽已选择另一技能；本场不能替换已学法术");
    }
    if(Product==20)Product=6;
    if(Product<8)
    {
        if(Product==1)return TEXT("天罚是初始技能，无需购买");
        int L=S->Level(Product),Cap=WarlockRules::LevelCap(Product)+(S->StoneSlot==Product?2:0);
        if(L>=Cap)return FString::Printf(TEXT("已达等级上限 %d"),Cap);
        int Sum=S->LifeMastery+S->RangeMastery+S->TimeMastery;
        int Need=Product==0?(L+1)/2:Product==5&&L>=4?1:0;
        if(Sum<Need)return FString::Printf(TEXT("需要精通总等级 %d（当前 %d）"),Need,Sum);
        Need=Product==2&&L>=3?(L<=4?1:L==5?2:3):Product==4&&L>=6?1:0;
        if(S->RangeMastery<Need)return FString::Printf(TEXT("需要远程精通 %d（当前 %d）"),Need,S->RangeMastery);
    }
    else if(Product<12)
    {
        int L=Product==8?S->LifeMastery:Product==9?S->RangeMastery:Product==10?S->TimeMastery:FMath::Max3(S->LifeMastery,S->RangeMastery,S->TimeMastery);
        if(L>=6)return Product==11?TEXT("至少一项精通已满级，不能购买组合"):TEXT("精通已达 6 级上限");
    }
    else
    {
        int I=Product-12;
        if(I==3&&S->bHasStone)return TEXT("乔丹之石每场只能购买一次，不可出售");
        if(S->Items[I]>=ItemCap[I])return FString::Printf(TEXT("已达装备 %d 级上限"),ItemCap[I]);
        if((I==1&&S->Items[4])||(I==4&&S->Items[1]))return TEXT("速度之靴与熔岩靴互斥，请先出售已有靴子");
        if(!S->Items[I]&&UsedSlots(S)>=2)return TEXT("两格装备已满；同类升级不额外占格");
    }
    return FString();
}
int RawPrice(const AWarlockState* S,int Product)
{
    if(Product<0||Product>20)return -1;
    if(Product==20)Product=6;
    if(Product<8)return Product==1?-1:S->Level(Product)?WarlockRules::UpgradeCost(Product,S->Level(Product)):WarlockRules::LearnCost(Product)+FMath::Max(0,S->LearnedCount()-2);
    if(Product<11)return (Product==8?6+S->LifeMastery:Product==9?5+S->RangeMastery:4+S->TimeMastery);
    return Product==11?13+S->LifeMastery+S->RangeMastery+S->TimeMastery:ItemCost[Product-12];
}
FString SkillStats(const AWarlockState* S,int Slot,int L)
{
    float R=1+.12f*(S->RangeMastery-S->Items[7]),T=1+.08f*S->TimeMastery-.05f*S->Items[0];
    bool Alt=S->Modes.IsValidIndex(Slot)&&S->Modes[Slot];
    float CD=WarlockRules::Cooldown(Slot,L,Alt);
    switch(Slot)
    {
    case 0:return FString::Printf(TEXT("伤害 %.1f · 射程 %.0f\n冷却 %.2f 秒"),7+.5f*(L-1),1000*R,CD);
    case 1:return FString::Printf(TEXT("伤害 %d · 半径 250\n自损 10 HP · 冷却 3 秒"),10+S->Items[2]);
    case 2:return FString::Printf(TEXT("伤害 %d · 射程 %.0f\n冷却 %.2f 秒"),6+L,600*R,CD);
    case 3:return Alt?FString::Printf(TEXT("折跃 %.0f + 300\n两段窗口 3.3 秒 · 冷却 %.2f 秒"),300.f+50*L,CD):FString::Printf(TEXT("闪现 %.0f\n冷却 %.2f 秒"),700.f+70*L,CD);
    case 4:return FString::Printf(TEXT("%s %d 发 × %.1f 伤害\n冷却 %.2f 秒"),Alt?TEXT("齐射"):TEXT("连珠"),Alt?4:5+L,(Alt?3.3f:2.3f)+.3f*L,CD);
    case 5:return FString::Printf(TEXT("中心伤害 %d · 最大距离 1000\n落地延迟 1.15 秒 · 冷却 %.2f 秒"),10+L,CD);
    case 6:return FString::Printf(TEXT("反射持续 %.2f 秒\n冷却 %.2f 秒"),(2.6f+.2f*L)*T,CD);
    case 20:return FString::Printf(TEXT("持续 %.2f 秒 · 吸收池 %d\n初始移速 +35 · 冷却 %.2f 秒"),(5.5f+.5f*L)*T,3+2*L,22.f-L);
    default:return FString::Printf(TEXT("敌人拉向自己 / 柱子拉近自身\n飞行距离 %.0f · 冷却 %.2f 秒"),950*.86f*R,CD);
    }
}
FString ItemStats(int I,int L)
{
    static const float Lava[]={0,13,24,35},Speed[]={0,20,30,40},Regen[]={0,.2f,.35f,.45f},Helmet[]={1,.84f,.76f,.68f};
    switch(I)
    {
    case 0:return L?TEXT("负面持续 ×0.65\n自身持续倍率 −0.05"):TEXT("无抗魔斗篷效果");
    case 1:return FString::Printf(TEXT("移速 +%.0f · 吸血负抗 +%d%%\n岩浆天罚保护 %d 秒，冷却 28 秒"),Lava[L],L?5:0,L?2+L:0);
    case 2:return FString::Printf(TEXT("天罚伤害 %d\n每次有效命中清除 K %d"),10+L,L?1+L:0);
    case 3:return TEXT("占用一格 · 仅一次 · 不可出售\n选择已学习技能后，上限 +2\n不赠送等级，升级仍需花费金币");
    case 4:return FString::Printf(TEXT("移速 +%.0f\n与熔岩靴互斥"),Speed[L]);
    case 5:return FString::Printf(TEXT("额外回复 %.2f HP/秒\n吸血负抗 +%d%%"),Regen[L],L==1?6:L?10:0);
    case 6:return FString::Printf(TEXT("最大生命 +%d · 吸血负抗 +%d%%\n额外回复 %.2f HP/秒"),10*L,5*L,L==3?.1f:0.f);
    default:return FString::Printf(TEXT("所受击退 ×%.2f\n有效远程精通 −%d"),Helmet[L],L);
    }
}
}

int AWarlockMode::Price(AWarlockPawn* P,int Product) const
{
    auto S=P?P->State():nullptr;
    return !S||!Gate(S,Product).IsEmpty()?-1:RawPrice(S,Product);
}
int AWarlockMode::NextCost(AWarlockPawn* P,int Product) const
{
    return P&&P->State()?RawPrice(P->State(),Product):-1;
}
FString AWarlockMode::PurchaseReason(AWarlockPawn* P,int Product) const
{
    if(!InShop(P))return TEXT("只能在准备阶段购买");
    auto S=P->State();FString Why=Gate(S,Product);if(!Why.IsEmpty())return Why;
    int Cost=RawPrice(S,Product);
    return S->Gold<Cost?FString::Printf(TEXT("还需 %d 金币（价格 %d，持有 %d）"),Cost-S->Gold,Cost,S->Gold):FString();
}
bool AWarlockMode::Buy(AWarlockPawn* P,int Product,FString& Reason)
{
    if(!HasAuthority()){Reason=TEXT("购买必须由服务器处理");return false;}
    Reason=PurchaseReason(P,Product);if(!Reason.IsEmpty())return false;
    auto S=P->State();int Cost=RawPrice(S,Product);S->Gold-=Cost;S->bReady=S->bBot;
    if(Product==20){S->DefenseChoice=1;++S->Levels[6];}
    else if(Product<8)++S->Levels[Product];
    else if(Product==8)++S->LifeMastery;
    else if(Product==9)++S->RangeMastery;
    else if(Product==10)++S->TimeMastery;
    else if(Product==11){++S->LifeMastery;++S->RangeMastery;++S->TimeMastery;}
    else {++S->Items[Product-12];if(Product==15)S->bHasStone=true;}
    P->Health=P->MaxHealth();S->ForceNetUpdate();P->ForceNetUpdate();
    Reason=Product==15?TEXT("乔丹之石已购买，请选择已学技能槽启用"):FString::Printf(TEXT("购买成功 · 花费 %d · 余额 %d"),Cost,S->Gold);return true;
}
int AWarlockMode::SalePrice(AWarlockPawn* P,int Product) const
{
    if(!P||!P->State()||Product<12||Product>=20||Product==15)return -1;
    int L=P->State()->Items[Product-12];return L?ItemRefund[Product-12]*L:-1;
}
bool AWarlockMode::Sell(AWarlockPawn* P,int Product,FString& Reason)
{
    if(!HasAuthority()||!InShop(P)){Reason=TEXT("只能在准备阶段出售");return false;}
    int Refund=SalePrice(P,Product);if(Refund<0){Reason=Product==15?TEXT("乔丹之石不可出售"):TEXT("没有可出售的装备；技能与精通不能退款");return false;}
    auto S=P->State();S->Gold+=Refund;S->Items[Product-12]=0;S->bReady=S->bBot;
    if(Product==13)P->LavaProtection=0; // BUG06: remove the sold boots' active instance.
    P->Health=FMath::Min(P->Health,P->MaxHealth());S->ForceNetUpdate();P->ForceNetUpdate();
    Reason=FString::Printf(TEXT("整件出售 · 返还 %d · 余额 %d"),Refund,S->Gold);return true;
}
bool AWarlockMode::AssignStone(AWarlockPawn* P,int Slot,FString& Reason)
{
    if(Slot==20)Slot=6;
    if(!HasAuthority()||!InShop(P)){Reason=TEXT("只能在准备阶段启用乔丹之石");return false;}
    auto S=P->State();
    if(!S->bHasStone||S->StoneSlot>=0||Slot<0||Slot>=8||Slot==1||!S->Level(Slot))
    {Reason=TEXT("需要未启用的乔丹之石，并选择已学技能（天罚除外）");return false;}
    S->StoneSlot=Slot;S->bReady=S->bBot;S->ForceNetUpdate();Reason=TEXT("该技能上限已增加两级；仍需正常购买升级");return true;
}
bool AWarlockMode::SetSkillMode(AWarlockPawn* P,int Slot,FString& Reason)
{
    if(!HasAuthority()||!InShop(P)){Reason=TEXT("只能在准备阶段切换效果模式");return false;}
    auto S=P->State();
    if((Slot!=3&&Slot!=4)||!S->Level(Slot)){Reason=TEXT("当前开放已学瞬间移动与火焰喷射的双模式");return false;}
    S->Modes[Slot]=!S->Modes[Slot];S->bReady=S->bBot;S->ForceNetUpdate();
    Reason=TEXT("效果模式已切换 · 免费 · 等级不变");return true;
}
FString AWarlockMode::ProductDetails(AWarlockPawn* P,int Product) const
{
    auto S=P?P->State():nullptr;if(!S||Product<0||Product>20)return FString();
    FString Result;
    if(Product<8||Product==20)
    {
        int Slot=Product==20?6:Product;
        bool Other=Slot==6&&S->Level(6)&&S->DefenseChoice!=(Product==20?1:0);
        int L=Other?0:S->Level(Slot),Cap=WarlockRules::LevelCap(Slot)+(S->StoneSlot==Slot?2:0);
        Result=FString::Printf(TEXT("等级 %d / %d%s\n"),L,Cap,S->StoneSlot==Slot?TEXT(" · 石头 +2"):TEXT(""));
        if(L)Result+=TEXT("当前  ")+SkillStats(S,Product,L)+TEXT("\n");
        if(Product!=1&&L<Cap)Result+=FString::Printf(TEXT("%s  "),L?TEXT("下一级"):TEXT("学会后"))+SkillStats(S,Product,L+1);
        if(!L)Result+=FString::Printf(TEXT("\n学习原价 %d + 多技能附加 %d"),WarlockRules::LearnCost(Slot),FMath::Max(0,S->LearnedCount()-2));
    }
    else if(Product<12)
    {
        int Life=S->LifeMastery,Range=S->RangeMastery,Time=S->TimeMastery;
        Result=FString::Printf(TEXT("当前精通  生命 %d / 远程 %d / 时间 %d\n"),Life,Range,Time);
        if(Product==8||Product==11)Result+=FString::Printf(TEXT("吸血 %.0f%% → %.0f%%\n"),Life*10.f,(Life+1)*10.f);
        if(Product==9||Product==11)Result+=FString::Printf(TEXT("常规射程倍率 %.2f → %.2f\n"),1+.12f*(Range-S->Items[7]),1+.12f*(Range+1-S->Items[7]));
        if(Product==10||Product==11)Result+=FString::Printf(TEXT("持续倍率 %.2f → %.2f\n"),1+.08f*Time-.05f*S->Items[0],1+.08f*(Time+1)-.05f*S->Items[0]);
        Result+=FString::Printf(TEXT("回复倍率 %.2f → %.2f\n所受击退精通倍率 %.3f → %.3f"),1+.05f*(Life+Range+Time),1+.05f*(Life+Range+Time+(Product==11?3:1)),1-.025f*(Life+Range+Time),1-.025f*(Life+Range+Time+(Product==11?3:1)));
    }
    else
    {
        int I=Product-12,L=S->Items[I];Result=FString::Printf(TEXT("装备等级 %d / %d\n"),L,ItemCap[I]);
        if(I==3)Result+=ItemStats(I,1);
        else {Result+=TEXT("当前  ")+ItemStats(I,L);if(L<ItemCap[I])Result+=TEXT("\n下一级  ")+ItemStats(I,L+1);}
    }
    return Result;
}
