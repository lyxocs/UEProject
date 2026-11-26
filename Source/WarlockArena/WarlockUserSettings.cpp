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
template<typename T> T NextValue(const TArray<T>& Values,T Current,int Direction)
{
    int Index=Values.IndexOfByKey(Current);
    if(Index==INDEX_NONE)Index=Direction>0?-1:0;
    return Values[(Index+(Direction>0?1:-1)+Values.Num())%Values.Num()];
}
}
void UWarlockUserSettings::ApplyNonResolutionSettings()
{
    Super::ApplyNonResolutionSettings();
    if(auto Method=IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod")))
        Method->Set(GetAntiAliasingQuality()>=3?4:2,ECVF_SetByCode);
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
    SetAntiAliasingQuality(GraphicsPreset==4?3:2);SetTextureQuality(GraphicsPreset==0?1:2);
    const float Scales[]={67,75,85,100,100};
    const int BudgetWidth=GraphicsPreset<=2?1920:GraphicsPreset==3?2560:3840;
    int OutputWidth=GetScreenResolution().X;
    // Standalone diagnostics can override the stored window size through -ResX.
    if(bClientSettingsInitialized&&!GIsEditor&&GEngine&&GEngine->GameViewport&&GEngine->GameViewport->Viewport)
        OutputWidth=GEngine->GameViewport->Viewport->GetSizeXY().X;
    float Scale=FMath::Min(Scales[GraphicsPreset],100.f*BudgetWidth/FMath::Max(1,OutputWidth));
    SetResolutionScaleValueEx(FMath::Clamp(Scale,50.f,100.f));SetDynamicResolutionEnabled(false);
}
const TCHAR* UWarlockUserSettings::RowName(int32 Row)
{
    static const TCHAR* Names[]={TEXT("画质预设"),TEXT("窗口分辨率"),TEXT("显示模式"),TEXT("场景渲染比例"),TEXT("帧率上限"),TEXT("垂直同步"),TEXT("阴影质量"),TEXT("全局光照"),TEXT("反射质量"),TEXT("特效质量")};
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
    }
    SetDynamicResolutionEnabled(false);
    if(Row==1||Row==2){ApplySettings(false);ConfirmVideoMode();}else ApplyNonResolutionSettings();
    bClientSettingsInitialized=true;SaveSettings();
}
