#include "WarlockCatalog.h"
#include "WarlockCatalogData.h"
#include "WarlockGame.h"
namespace WarlockCatalog
{
using namespace WarlockCatalogData;
int Family(const AWarlockState* S,int Slot)
{
    if(!S||Slot<0||Slot>=8)return -1;
    if(Slot==6&&S->DefenseChoice==1)return Haste;
    return S->Families.IsValidIndex(Slot)&&S->Families[Slot]>=0&&S->Families[Slot]<Count&&Entries[S->Families[Slot]].Slot==Slot?S->Families[Slot]:Slot;
}
int Product(int F){return F<8?F:F==Haste?20:100+F;}
int FromProduct(int P){return P>=0&&P<8?P:P==20?Haste:P>=109&&P<100+Count?P-100:-1;}
int Slot(int F){return F>=0&&F<Count?Entries[F].Slot:-1;}
const TCHAR* Name(int F){return F>=0&&F<Count?Entries[F].Name:TEXT("未知技能");}
float Cooldown(int F,int L,bool Alt){return F>=0&&F<Count?Entries[F].CD[FMath::Clamp(L-1,0,9)+(Alt&&Dual(F)?10:0)]:0;}
int Cap(int F){return F>=0&&F<Count?Entries[F].Cap:0;}
int Cost(int F,int L,int Learned){if(F<0||F>=Count||F==Smite)return -1;return L?Entries[F].Upgrade+L-1:Entries[F].Learn+FMath::Max(0,Learned-2);}
bool Dual(int F){return F>=0&&F<Count&&Entries[F].Dual;}
FString Gate(const AWarlockState* S,int F)
{
    if(!S||F<0||F>=Count)return TEXT("无效技能");
    int I=Slot(F),L=S->Level(I),Current=Family(S,I);
    if(L>0&&Current!=F)return TEXT("此槽已学习其他技能，本场不可替换");
    if(F==Smite)return TEXT("初始技能，无需购买");
    if(L>=Cap(F)+(S->StoneSlot==I?2:0))return TEXT("已达等级上限");
    const auto& E=Entries[F];int R=E.RangeGate[FMath::Clamp(L,0,9)],M=E.MasteryGate[FMath::Clamp(L,0,9)];
    if(F==Fireball)M=(L+1)/2;
    if(S->RangeMastery<R)return FString::Printf(TEXT("需要远程精通 %d"),R);
    if(S->LifeMastery+S->RangeMastery+S->TimeMastery<M)return FString::Printf(TEXT("需要精通总等级 %d"),M);
    return FString();
}
TArray<int> Families(int I){TArray<int> R;for(int F=0;F<Count;++F)if(Slot(F)==I)R.Add(F);return R;}
FString ModeName(int F,bool A)
{
    switch(F){case Blink:return A?TEXT("折跃"):TEXT("闪现");case Dash:return A?TEXT("驭风"):TEXT("冲撞");case Swap:return A?TEXT("换位"):TEXT("移形");case Drain:return A?TEXT("虚弱"):TEXT("减速");case Spray:return A?TEXT("齐射"):TEXT("连珠");case Bounce:return A?TEXT("充能"):TEXT("弹射");case Meteor:return A?TEXT("滚石"):TEXT("陨石");case WindWalk:return A?TEXT("潜行"):TEXT("影袭");case Split:return A?TEXT("散华"):TEXT("爆裂");case Chain:return A?TEXT("红链"):TEXT("蓝链");case Bind:return A?TEXT("沉默"):TEXT("缠绕");case Gravity:return A?TEXT("力场"):TEXT("黑洞");case Control:return A?TEXT("点阵"):TEXT("扰乱");default:return Name(F);}
}
FString Description(int F,int L,bool A)
{
    switch(F)
    {
    case Boomerang:return FString::Printf(TEXT("基础伤害 %.1f · 命中后返回\n连续命中增加伤害并减少推力"),6.3f+.7f*L);
    case Seeker:return FString::Printf(TEXT("基础伤害 %d · 追踪目标\n飞行后半程提高伤害"),6+L);
    case Dash:return A?TEXT("短时驭风 · 质量 −60 千克\n变向追加冲量，方向由移动指令控制"):FString::Printf(TEXT("冲撞伤害 %.1f · 按速度平方衰减"),5+.4f*L);
    case Swap:return A?TEXT("交换自身与目标投射物的位置\n成功后获得短时移速"):TEXT("命中后交换角色位置\n未命中时移动至飞行终点");
    case Drain:return FString::Printf(TEXT("命中伤害 %d · 返还治疗球\n%s"),(A?4:5)+L,A?TEXT("敌方输出减半；友军获得输出增益"):TEXT("敌方减速 50；友军加速 50"));
    case Bounce:return FString::Printf(TEXT("基础伤害 %.1f\n%s"),A?5.3f+.7f*L:4.6f+.8f*L,A?TEXT("连续命中充能，命中重置本槽冷却"):TEXT("每次弹射伤害 ×0.7，最低 ×0.2"));
    case WindWalk:return FString::Printf(TEXT("%s · 移速 +%.0f\n持续 %.1f 秒 × 时间倍率"),A?TEXT("潜行碰触"):TEXT("影袭接触伤害"),A?200.f/3:200.f,A?4.f:2.6f);
    case Split:return FString::Printf(TEXT("母弹命中 3 · 子弹 %.1f\n%s"),2.5f+.5f*L,A?TEXT("沿途旋转散射子弹"):TEXT("到达后分裂成对的子弹"));
    case Blast:return FString::Printf(TEXT("单发 %.1f · 每 0.18 秒发射\n持续 2.8 秒 × 时间倍率"),2.7f+.3f*L);
    case Rewind:return FString::Printf(TEXT("施放时建立快照，3.5 秒后返回\n再次使用可提前返回\n额外击退保留 %.0f%%"),FMath::Max(0.f,.8f-.1f*L)*100);
    case Mirror:return FString::Printf(TEXT("4 秒内用移动 / 技能指令成形\n两道非实体分身 · 持续 4 秒\n分身火球伤害 %.1f"),.5f*L);
    case Bind:return A?TEXT("逐槽沉默 · F 天罚除外\n时间取决于双方各槽等级差"):FString::Printf(TEXT("缠绕 %.2f 秒 × 双方持续倍率\n期间仍可施法，自损额外 5 HP"),4+.25f*L);
    case Gravity:return A?TEXT("力场半径 200 · 持续 5 秒\n治疗友军并伤害、减速敌军"):FString::Printf(TEXT("牵引敌军与投射物\n近距离每 0.18 秒伤害 %.1f"),.1f+.2f*L);
    case Control:return A?TEXT("留下标记，最长保留 120 秒\n下一次可用技能从标记施放\n临时精通不会增加购买价格"):TEXT("扰乱目标施法朝向\n降低时间倍率，增加吸血负抗");
    default:return TEXT("");
    }
}
}
