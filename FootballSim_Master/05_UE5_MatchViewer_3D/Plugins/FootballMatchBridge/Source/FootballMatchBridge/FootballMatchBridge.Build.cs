using UnrealBuildTool;
public class FootballMatchBridge : ModuleRules
{
    public FootballMatchBridge(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.Add("Core");
        PrivateDependencyModuleNames.Add("Json");
    }
}
