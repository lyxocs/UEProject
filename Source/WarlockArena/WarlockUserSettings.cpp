#include "WarlockUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "DynamicRHI.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Misc/App.h"

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
{ApplyKeyboardPreset(0);}
void UWarlockUserSettings::ApplyKeyboardPreset(int32 Preset)
{
    if(Preset<0||Preset>1)return;
    KeyboardBindings.Reset();for(int I=0;I<BindingCount;++I)KeyboardBindings.Add(DefaultKey(I));
    if(Preset==1){const FKey Skills[]={EKeys::Q,EKeys::R,EKeys::W,EKeys::F,EKeys::E,EKeys::D,EKeys::C,EKeys::T};for(int I=0;I<8;++I)KeyboardBindings[I+4]=Skills[I];KeyboardBindings[17]=EKeys::V;}
    SaveSettings();
}
int32 UWarlockUserSettings::KeyboardPreset() const
{
    bool Classic=true,League=true;const FKey Skills[]={EKeys::Q,EKeys::R,EKeys::W,EKeys::F,EKeys::E,EKeys::D,EKeys::C,EKeys::T};
    for(int I=0;I<BindingCount;++I){Classic&=KeyboardKey(I)==DefaultKey(I);League&=KeyboardKey(I)==(I>=4&&I<12?Skills[I-4]:I==17?EKeys::V:DefaultKey(I));}
    return Classic?0:League?1:2;
}
void UWarlockUserSettings::ApplyNonResolutionSettings()
{
    AntiAliasingMode=FMath::Clamp(AntiAliasingMode,0,2);
    MusicVolume=FMath::Clamp(MusicVolume,0.f,1.f);EffectsVolume=FMath::Clamp(EffectsVolume,0.f,1.f);
    CastTriggerMode=FMath::Clamp(CastTriggerMode,0,1);
    // A bounded TAAU governor uses the selected frame cap (up to 120 FPS).
    // Keep fixed-scale choices independent from the automatic budget.
    SetDynamicResolutionEnabled(false);
    SetAntiAliasingQuality(AntiAliasingMode==1?2:3);
    Super::ApplyNonResolutionSettings();
    if(auto Method=IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod")))
        Method->Set(AntiAliasingMode==0?2:4,ECVF_SetByCode);
    // This arena's modest meshes render more cheaply through the raster path.
    // Keep lighting, material quality and native resolution independent of it.
    auto Budget=[](const TCHAR* Name,int Value){if(auto C=IConsoleManager::Get().FindConsoleVariable(Name))C->Set(Value,ECVF_SetByCode);};
    Budget(TEXT("r.Nanite"),0);
    Budget(TEXT("r.Shadow.Virtual.Enable"),0);
    Budget(TEXT("r.Shadow.CSM.MaxCascades"),GetShadowQuality()>=2?2:1);
    Budget(TEXT("r.Shadow.MaxCSMResolution"),GetShadowQuality()>=3?4096:GetShadowQuality()>=1?2048:1024);
    Budget(TEXT("r.DynamicRes.OperationMode"),0);
    auto FloatBudget=[](const TCHAR* Name,float Value){if(auto C=IConsoleManager::Get().FindConsoleVariable(Name))C->Set(Value,ECVF_SetByCode);};
    FloatBudget(TEXT("r.DynamicRes.FrameTimeBudget"),1000.f/(GetFrameRateLimit()>0?FMath::Min(120.f,GetFrameRateLimit()):120.f));
    FloatBudget(TEXT("r.DynamicRes.MinScreenPercentage"),50);
    FloatBudget(TEXT("r.DynamicRes.MaxScreenPercentage"),100);
    RuntimeRenderPercentage=ScalabilityQuality.ResolutionQuality;
    if(bAutoRenderBudget){int Width=GetScreenResolution().X;if(!GIsEditor&&GEngine&&GEngine->GameViewport&&GEngine->GameViewport->Viewport)Width=GEngine->GameViewport->Viewport->GetSizeXY().X;RuntimeRenderPercentage=Width>=3000?67.f:100.f;}
    FloatBudget(TEXT("r.ScreenPercentage"),RuntimeRenderPercentage);
    BudgetSampleTime=SmoothedGpuMs=0;
    if(auto C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Tonemapper.Sharpen")))C->Set(.2f,ECVF_SetByCode);
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
    bAutoRenderBudget=GraphicsPreset==3;
    const int Levels[]={0,1,1,2,3};SetOverallScalabilityLevel(Levels[GraphicsPreset]);
    if(GraphicsPreset==1){SetGlobalIlluminationQuality(0);SetReflectionQuality(0);SetPostProcessingQuality(0);}
    // Keep temporal reconstruction, readable materials and gameplay effects in every preset.
    AntiAliasingMode=0;
    SetAntiAliasingQuality(AntiAliasingMode==1?2:3);SetTextureQuality(GraphicsPreset==0?1:2);
    const float Scales[]={67,85,100,100,100};
    const int BudgetWidth=GraphicsPreset==0?1600:GraphicsPreset==1?1920:GraphicsPreset==2?2560:3840;
    int OutputWidth=GetScreenResolution().X;
    // Standalone diagnostics can override the stored window size through -ResX.
    if(bClientSettingsInitialized&&!GIsEditor&&GEngine&&GEngine->GameViewport&&GEngine->GameViewport->Viewport)
        OutputWidth=GEngine->GameViewport->Viewport->GetSizeXY().X;
    float Scale=FMath::Min(Scales[GraphicsPreset],100.f*BudgetWidth/FMath::Max(1,OutputWidth));
    SetResolutionScaleValueEx(FMath::Clamp(Scale,50.f,100.f));SetDynamicResolutionEnabled(bAutoRenderBudget);
}
const TCHAR* UWarlockUserSettings::RowName(int32 Row)
{
    static const TCHAR* Names[]={TEXT("画质预设"),TEXT("窗口分辨率"),TEXT("显示模式"),TEXT("场景渲染比例"),TEXT("帧率上限"),TEXT("垂直同步"),TEXT("阴影质量"),TEXT("全局光照"),TEXT("反射质量"),TEXT("特效质量"),TEXT("抗锯齿 / 重建"),TEXT("背景音乐音量"),TEXT("技能音效音量"),TEXT("指向技能触发"),TEXT("GPU 帧时间预算")};
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
    case 3:return bAutoRenderBudget?FString::Printf(TEXT("自动 %.0f%% · 范围 50～100%%"),RuntimeRenderPercentage):FString::Printf(TEXT("%.0f%% · UI 保持清晰"),ScalabilityQuality.ResolutionQuality);
    case 4:return GetFrameRateLimit()>0?FString::Printf(TEXT("%.0f FPS"),GetFrameRateLimit()):TEXT("不限帧");
    case 5:return IsVSyncEnabled()?TEXT("开启 · 受刷新率限制"):TEXT("关闭");
    case 6:return Quality[FMath::Clamp(GetShadowQuality(),0,3)];
    case 7:{const TCHAR* GI[]={TEXT("环境光 · 省性能"),TEXT("Lumen · 简化"),TEXT("Lumen · 高"),TEXT("Lumen · 精细")};return GI[FMath::Clamp(GetGlobalIlluminationQuality(),0,3)];}
    case 8:return Quality[FMath::Clamp(GetReflectionQuality(),0,3)];
    case 9:return Quality[FMath::Clamp(GetVisualEffectQuality(),0,3)];
    case 10:{const TCHAR* AA[]={TEXT("TAA · 高质量 / 兼容"),TEXT("TSR · 均衡"),TEXT("TSR · 精细")};return AA[FMath::Clamp(AntiAliasingMode,0,2)];}
    case 11:return FString::Printf(TEXT("%.0f%%"),MusicVolume*100);
    case 12:return FString::Printf(TEXT("%.0f%%"),EffectsVolume*100);
    case 13:return CastTriggerMode?TEXT("松开 / 左键确认"):TEXT("按下释放");
    case 14:return bAutoRenderBudget?FString::Printf(TEXT("自动 · 目标 %.0f FPS"),GetFrameRateLimit()>0?FMath::Min(120.f,GetFrameRateLimit()):120.f):TEXT("关闭 · 固定场景渲染比例");
    default:return TEXT("");
    }
}
void UWarlockUserSettings::UpdateRenderBudget(float Dt)
{
    if(!bAutoRenderBudget||GIsEditor||!FMath::IsFinite(Dt)||Dt<=0)return;
    const float RawGpuMs=FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
    // Include observed pacing as well as GPU work. Subtract deliberate
    // frame-cap sleep so a capped client doesn't reduce quality indefinitely.
    const float ActiveFrameMs=1000.f*FMath::Max(0.0,FApp::GetDeltaTime()-FApp::GetIdleTime());
    const float GpuMs=FMath::Max(RawGpuMs,FMath::Max(0.f,ActiveFrameMs-.8f));
    CSV_CUSTOM_STAT_GLOBAL(WarlockGpuTimerMs,RawGpuMs,ECsvCustomStatOp::Set);
    CSV_CUSTOM_STAT_GLOBAL(WarlockActiveFrameMs,ActiveFrameMs,ECsvCustomStatOp::Set);
    if(GpuMs<=0||GpuMs>100)return;
    SmoothedGpuMs=SmoothedGpuMs>0?FMath::Lerp(SmoothedGpuMs,GpuMs,1-FMath::Exp(-FMath::Min(Dt,.1f)*6)):GpuMs;
    BudgetSampleTime+=FMath::Min(Dt,.1f);
    if(BudgetSampleTime>=.25f)
    {
        BudgetSampleTime=0;
        const float TargetMs=1000.f/(GetFrameRateLimit()>0?FMath::Min(120.f,GetFrameRateLimit()):120.f)*.8f;
        if(FMath::Abs(SmoothedGpuMs-TargetMs)>.35f)
        {
            float Desired=RuntimeRenderPercentage*FMath::Sqrt(TargetMs/FMath::Max(1.f,SmoothedGpuMs));
            // Fast reductions, slow recovery, and a dead band keep the image stable.
            RuntimeRenderPercentage=FMath::Clamp(Desired,FMath::Max(50.f,RuntimeRenderPercentage-5.f),FMath::Min(100.f,RuntimeRenderPercentage+1.f));
            if(auto C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))){C->Set(RuntimeRenderPercentage,ECVF_SetByCode);RuntimeRenderPercentage=C->GetFloat();}
        }
    }
    CSV_CUSTOM_STAT_GLOBAL(WarlockRenderPercentage,RuntimeRenderPercentage,ECsvCustomStatOp::Set);
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
    case 3:bAutoRenderBudget=false;SetResolutionScaleValueEx(NextValue<float>({50,60,67,75,85,100},ScalabilityQuality.ResolutionQuality,Direction));GraphicsPreset=-1;break;
    case 4:SetFrameRateLimit(NextValue<float>({30,60,90,120,144,165,240,0},GetFrameRateLimit(),Direction));break;
    case 5:SetVSyncEnabled(!IsVSyncEnabled());break;
    case 6:SetShadowQuality((GetShadowQuality()+Direction+4)%4);GraphicsPreset=-1;break;
    case 7:SetGlobalIlluminationQuality((GetGlobalIlluminationQuality()+Direction+4)%4);GraphicsPreset=-1;break;
    case 8:SetReflectionQuality((GetReflectionQuality()+Direction+4)%4);GraphicsPreset=-1;break;
    case 9:SetVisualEffectQuality((GetVisualEffectQuality()+Direction+4)%4);GraphicsPreset=-1;break;
    case 10:AntiAliasingMode=(AntiAliasingMode+Direction+3)%3;GraphicsPreset=-1;break;
    case 11:MusicVolume=FMath::Clamp(MusicVolume+Direction*.1f,0.f,1.f);break;
    case 12:EffectsVolume=FMath::Clamp(EffectsVolume+Direction*.1f,0.f,1.f);break;
    case 13:CastTriggerMode=1-CastTriggerMode;break;
    case 14:bAutoRenderBudget=!bAutoRenderBudget;GraphicsPreset=-1;break;
    }
    SetDynamicResolutionEnabled(bAutoRenderBudget);
    if(Row==1||Row==2){ApplySettings(false);ConfirmVideoMode();}else ApplyNonResolutionSettings();
    bClientSettingsInitialized=true;SaveSettings();
}
