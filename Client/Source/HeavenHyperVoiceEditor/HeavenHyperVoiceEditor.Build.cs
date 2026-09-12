// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HeavenHyperVoiceEditor : ModuleRules
{
	public HeavenHyperVoiceEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"HeavenHyperVoice",
			"Landscape",
			"ToolMenus", "UnrealEd", "UMG", "UMGEditor", "Slate", "SlateCore", "Kismet", "KismetCompiler",
			"AssetRegistry", "MeshDescription", "StaticMeshDescription"
		});
	}
}
