using UnrealBuildTool;
public class FootballReplayViewer : ModuleRules
{
    public FootballReplayViewer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new [] { "Core", "CoreUObject", "Engine", "InputCore", "FootballMatchBridge" });
        PrivateDependencyModuleNames.AddRange(new [] { "Slate", "SlateCore", "Json" });
    }
}
