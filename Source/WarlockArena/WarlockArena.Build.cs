using UnrealBuildTool;
public class WarlockArena : ModuleRules
{
    public WarlockArena(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "EngineSettings", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore", "Niagara", "NetCore", "AIModule"
        });
        PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI", "AnimGraphRuntime", "Json" });
    }
}
