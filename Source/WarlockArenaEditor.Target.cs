using UnrealBuildTool;
public class WarlockArenaEditorTarget : TargetRules
{
    public WarlockArenaEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("WarlockArena");
    }
}
