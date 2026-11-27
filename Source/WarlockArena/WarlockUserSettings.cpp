#include "WarlockUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"

namespace
{
FKey DefaultKey(int32 Index)
{
    const FKey Keys[]={EKeys::Up,EKeys::Down,EKeys::Left,EKeys::Right,EKeys::G,EKeys::F,EKeys::D,EKeys::R,EKeys::T,EKeys::E,EKeys::C,EKeys::Y,EKeys::Tab,EKeys::S,EKeys::SpaceBar,EKeys::Enter,EKeys::M,EKeys::Q,EKeys::Delete,EKeys::J,EKeys::H,EKeys::F1};
    return Index>=0&&Index<UWarlockUserSettings::BindingCount?Keys[Index]:FKey();
}
template<typename T> T NextValue(const TArray<T>& Values,T Current,int Direction)
{
    int Index=Values.IndexOfByKey(Current);
    if(Index==INDEX_NONE)Index=Direction>0?-1:0;
    return Values[(Index+(Direction>0?1:-1)+Values.Num())%Values.Num()];
}
}
UWarlockUserSettings::UWarlockUserSettings()
{for(int I=0;I<BindingCount;++I)KeyboardBindings.Add(DefaultKey(I));}
FKey UWarlockUserSettings::KeyboardKey(int32 Index) const
{return KeyboardBindings.IsValidIndex(Index)&&KeyboardBindings[Index].IsValid()?KeyboardBindings[Index]:DefaultKey(Index);}
const TCHAR* UWarlockUserSettings::BindingName(int32 Index)
{
    const TCHAR* Names[]={TEXT("向上移动"),TEXT("向下移动"),TEXT("向左移动"),TEXT("向右移动"),TEXT("火球"),TEXT("天罚"),TEXT("闪电"),TEXT("瞬间移动"),TEXT("火焰喷射"),TEXT("流星"),TEXT("反射盾 / 急行"),TEXT("锁链"),TEXT("打开商店"),TEXT("停止 / 取消施法"),TEXT("准备完毕"),TEXT("购买 / 确认"),TEXT("技能模式"),TEXT("查看替代路线"),TEXT("出售装备"),TEXT("启用乔丹之石"),TEXT("切换外观"),TEXT("操作指南")};
    return Index>=0&&Index<BindingCount?Names[Index]:TEXT("");
}
const TCHAR* UWarlockUserSettings::ActionName(int32 Index)
{
    const TCHAR* Names[]={TEXT("NavUp"),TEXT("NavDown"),TEXT("NavLeft"),TEXT("NavRight"),TEXT("Cast0"),TEXT("Cast1"),TEXT("Cast2"),TEXT("Cast3"),TEXT("Cast4"),TEXT("Cast5"),TEXT("Cast6"),TEXT("Cast7"),TEXT("Shop"),TEXT("Stop"),TEXT("Ready"),TEXT("Confirm"),TEXT("Mode"),TEXT("Alternative"),TEXT("Sell"),TEXT("Stone"),TEXT("Appearance"),TEXT("Help")};
    return Index>=0&&Index<BindingCount?Names[Index]:TEXT("");
}
bool UWarlockUserSettings::SetKeyboardBinding(int32 Index,FKey Key)
{
    if(Index<0||Index>=BindingCount||!Key.IsValid()||Key.IsGamepadKey()||Key.IsMouseButton()||Key.IsAxis1D()||Key.IsAxis2D()||Key.IsAxis3D()||Key.IsModifierKey())return false;
    for(auto Reserved:{EKeys::Escape,EKeys::BackSpace,EKeys::Tilde,EKeys::F2,EKeys::F3,EKeys::F4,EKeys::F5,EKeys::F6,EKeys::F7,EKeys::F8,EKeys::F9,EKeys::F10,EKeys::PageUp,EKeys::PageDown})if(Key==Reserved)return false;
    if(KeyboardBindings.Num()!=BindingCount){KeyboardBindings.Reset();for(int I=0;I<BindingCount;++I)KeyboardBindings.Add(DefaultKey(I));}
    FKey Previous=KeyboardBindings[Index];
    for(int I=0;I<BindingCount;++I)if(I!=Index&&KeyboardBindings[I]==Key)KeyboardBindings[I]=Previous;
    KeyboardBindings[Index]=Key;SaveSettings();return true;
}
void UWarlockUserSettings::ResetKeyboardBindings()
{KeyboardBindings.Reset();for(int I=0;I<BindingCount;++I)KeyboardBindings.Add(DefaultKey(I));SaveSettings();}
void UWarlockUserSettings::ApplyNonResolutionSettings()
{
    AntiAliasingMode=FMath::Clamp(AntiAliasingMode,0,2);
    SetAntiAliasingQuality(AntiAliasingMode==1?2:3);
    Super::ApplyNonResolutionSettings();
    if(auto Method=IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod")))
        Method->Set(AntiAliasingMode==0?2:4,ECVF_SetByCode);
}
void UWarlockUserSettings::InitializeClient()
{
    if(bClientSettingsInitialized){ApplyNonResolutionSettings();return;}
    int LegacyRevision=0;GConfig->GetInt(TEXT("WarlockDisplay"),TEXT("Revision"),LegacyRevision,GGameUserSettingsIni);
    if(LegacyRevision>=3)
    {
        // Preserve existing players' choices when moving to the derived settings class.
        const TCHAR* Section=TEXT("/Script/Engine.GameUserSettings");int X=0,Y=0,Mode=1;float Cap=60;bool Sync=false;
        GConfig->GetInt(Section,TEXT("ResolutionSizeX"),X,GGameUserSettingsIni);GConfig->GetInt(Section,TEXT("ResolutionSizeY"),Y,GGameUserSettingsIni);
        GConfig->GetInt(Section,TEXT("FullscreenMode"),Mode,GGameUserSettingsIni);GConfig->GetFloat(Section,TEXT("FrameRateLimit"),Cap,GGameUserSettingsIni);GConfig->GetBool(Section,TEXT("bUseVSync"),Sync,GGameUserSettingsIni);
        if(X>0&&Y>0)SetScreenResolution(FIntPoint(X,Y));
        SetFullscreenMode(EWindowMode::ConvertIntToWindowMode(Mode));SetFrameRateLimit(Cap);SetVSyncEnabled(Sync);GraphicsPreset=-1;
    }
    else
    {
        SetScreenResolution(GetDesktopResolution());SetFullscreenMode(EWindowMode::WindowedFullscreen);SetFrameRateLimit(120);SetVSyncEnabled(false);SelectPreset(2);
    }
    bClientSettingsInitialized=true;
    int ExplicitWidth=0;
    // Profiling/launch arguments keep control of their output dimensions.
    if(FParse::Value(FCommandLine::Get(),TEXT("ResX="),ExplicitWidth))ApplyNonResolutionSettings();else ApplySettings(false);
    SaveSettings();
}
const TCHAR* UWarlockUserSettings::PresetName(int32 Preset)
{
    static const TCHAR* Names[]={TEXT("低配"),TEXT("流畅"),TEXT("均衡"),TEXT("高画质"),TEXT("精细")};
    return Preset>=0&&Preset<5?Names[Preset]:TEXT("自定义");
}
void UWarlockUserSettings::SelectPreset(int32 Preset)
{
    GraphicsPreset=FMath::Clamp(Preset,0,4);
    const int Levels[]={0,1,1,2,3};SetOverallScalabilityLevel(Levels[GraphicsPreset]);
    if(GraphicsPreset==1){SetGlobalIlluminationQuality(0);SetReflectionQuality(0);SetPostProcessingQuality(0);}
    // Keep temporal reconstruction, readable materials and gameplay effects in every preset.
    AntiAliasingMode=GraphicsPreset<=1?0:GraphicsPreset==4?2:1;
    SetAntiAliasingQuality(AntiAliasingMode==1?2:3);SetTextureQuality(GraphicsPreset==0?1:2);
    const float Scales[]={67,85,100,100,100};
    const int BudgetWidth=GraphicsPreset==0?1600:GraphicsPreset==1?1920:GraphicsPreset==2?2560:3840;
    int OutputWidth=GetScreenResolution().X;
    // Standalone diagnostics can override the stored window size through -ResX.
    if(bClientSettingsInitialized&&!GIsEditor&&GEngine&&GEngine->GameViewport&&GEngine->GameViewport->Viewport)
        OutputWidth=GEngine->GameViewport->Viewport->GetSizeXY().X;
    float Scale=FMath::Min(Scales[GraphicsPreset],100.f*BudgetWidth/FMath::Max(1,OutputWidth));
    SetResolutionScaleValueEx(FMath::Clamp(Scale,50.f,100.f));SetDynamicResolutionEnabled(false);
}
const TCHAR* UWarlockUserSettings::RowName(int32 Row)
{
    static const TCHAR* Names[]={TEXT("画质预设"),TEXT("窗口分辨率"),TEXT("显示模式"),TEXT("场景渲染比例"),TEXT("帧率上限"),TEXT("垂直同步"),TEXT("阴影质量"),TEXT("全局光照"),TEXT("反射质量"),TEXT("特效质量"),TEXT("抗锯齿 / 重建")};
    return Row>=0&&Row<RowCount?Names[Row]:TEXT("");
}
FString UWarlockUserSettings::Label(int32 Row) const
{
    const TCHAR* Quality[]={TEXT("低 / 关闭"),TEXT("中"),TEXT("高"),TEXT("精细")};
    switch(Row)
    {
    case 0:return PresetName(GraphicsPreset);
    case 1:{auto R=GetScreenResolution();return FString::Printf(TEXT("%d × %d"),R.X,R.Y);}
    case 2:return GetFullscreenMode()==EWindowMode::Windowed?TEXT("窗口"):TEXT("无边框 · 桌面分辨率");
    case 3:return FString::Printf(TEXT("%.0f%% · UI 保持清晰"),ScalabilityQuality.ResolutionQuality);
    case 4:return GetFrameRateLimit()>0?FString::Printf(TEXT("%.0f FPS"),GetFrameRateLimit()):TEXT("不限帧");
    case 5:return IsVSyncEnabled()?TEXT("开启 · 受刷新率限制"):TEXT("关闭");
    case 6:return Quality[FMath::Clamp(GetShadowQuality(),0,3)];
    case 7:{const TCHAR* GI[]={TEXT("环境光 · 省性能"),TEXT("Lumen · 简化"),TEXT("Lumen · 高"),TEXT("Lumen · 精细")};return GI[FMath::Clamp(GetGlobalIlluminationQuality(),0,3)];}
    case 8:return Quality[FMath::Clamp(GetReflectionQuality(),0,3)];
    case 9:return Quality[FMath::Clamp(GetVisualEffectQuality(),0,3)];
    case 10:{const TCHAR* AA[]={TEXT("TAA · 高质量 / 兼容"),TEXT("TSR · 均衡"),TEXT("TSR · 精细")};return AA[FMath::Clamp(AntiAliasingMode,0,2)];}
    default:return TEXT("");
    }
}
void UWarlockUserSettings::Adjust(int32 Row,int32 Direction)
{
    if(Row<0||Row>=RowCount||Direction==0)return;
    Direction=Direction>0?1:-1;
    switch(Row)
    {
    case 0:SelectPreset((FMath::Max(0,GraphicsPreset)+Direction+5)%5);break;
    case 1:{auto Desktop=GetDesktopResolution();TArray<FIntPoint> Modes;
        for(auto R:{FIntPoint(1280,720),FIntPoint(1600,900),FIntPoint(1920,1080),FIntPoint(2560,1440),FIntPoint(3840,2160),Desktop})if(R.X<=Desktop.X&&R.Y<=Desktop.Y)Modes.AddUnique(R);
        if(!Modes.IsEmpty()){SetScreenResolution(NextValue(Modes,GetScreenResolution(),Direction));SetFullscreenMode(EWindowMode::Windowed);}break;}
    case 2:if(GetFullscreenMode()==EWindowMode::Windowed){SetScreenResolution(GetDesktopResolution());SetFullscreenMode(EWindowMode::WindowedFullscreen);}else SetFullscreenMode(EWindowMode::Windowed);break;
    case 3:SetResolutionScaleValueEx(NextValue<float>({50,67,75,85,100},ScalabilityQuality.ResolutionQuality,Direction));GraphicsPreset=-1;break;
    case 4:SetFrameRateLimit(NextValue<float>({30,60,90,120,144,165,240,0},GetFrameRateLimit(),Direction));break;
    case 5:SetVSyncEnabled(!IsVSyncEnabled());break;
    case 6:SetShadowQuality((GetShadowQuality()+Direction+4)%4);GraphicsPreset=-1;break;
    case 7:SetGlobalIlluminationQuality((GetGlobalIlluminationQuality()+Direction+4)%4);GraphicsPreset=-1;break;
    case 8:SetReflectionQuality((GetReflectionQuality()+Direction+4)%4);GraphicsPreset=-1;break;
    case 9:SetVisualEffectQuality((GetVisualEffectQuality()+Direction+4)%4);GraphicsPreset=-1;break;
    case 10:AntiAliasingMode=(AntiAliasingMode+Direction+3)%3;GraphicsPreset=-1;break;
    }
    SetDynamicResolutionEnabled(false);
    if(Row==1||Row==2){ApplySettings(false);ConfirmVideoMode();}else ApplyNonResolutionSettings();
    bClientSettingsInitialized=true;SaveSettings();
}
