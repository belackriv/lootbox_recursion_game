using UnrealBuildTool;

public class LootboxRecursion : ModuleRules
{
	public LootboxRecursion(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets code include headers relative to the module root, e.g. "Simulation/LRSimulation.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RHI",
			"RenderCore",
			"Slate",
			"SlateCore",
			"Json",
			"JsonUtilities",
			"MeshDescription",
			"StaticMeshDescription",
		});
	}
}
