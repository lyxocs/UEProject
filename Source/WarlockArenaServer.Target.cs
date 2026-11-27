using UnrealBuildTool;
public class WarlockArenaServerTarget : TargetRules
{
    public WarlockArenaServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("WarlockArena");
    }
}
