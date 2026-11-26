using UnrealBuildTool;
public class WarlockArenaTarget : TargetRules
{
    public WarlockArenaTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("WarlockArena");
    }
}
