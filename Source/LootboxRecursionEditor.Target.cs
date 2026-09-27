using UnrealBuildTool;

public class LootboxRecursionEditorTarget : TargetRules
{
	public LootboxRecursionEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LootboxRecursion");
	}
}
