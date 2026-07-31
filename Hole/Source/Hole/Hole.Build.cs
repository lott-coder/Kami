// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Hole : ModuleRules
{
	public Hole(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"LevelSequence",
				"MovieScene"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
