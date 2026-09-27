using UnrealBuildTool;

public class LootboxRecursionTarget : TargetRules
{
	public LootboxRecursionTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LootboxRecursion");
	}
}
