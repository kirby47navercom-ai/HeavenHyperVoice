// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class HeavenHyperVoice : ModuleRules
{
	public HeavenHyperVoice(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "../MovementCore")));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Net/Generated"));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.AddRange(new string[] { "Shell32.lib", "Ole32.lib" });
		}
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AnimGraphRuntime",
			"InputCore",
			"EnhancedInput",
			"AssetRegistry",
			"GameplayTags",
			"UMG",
			"GameplayAbilities",
			"GameplayTasks","GeometryCore",
			"GeometryFramework","ProceduralMeshComponent"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"MoviePlayer",
			"DeveloperSettings",
			"Landscape",

			// Field server transport. The engine ships OpenSSL 1.1.1t, which is
			// enough for TLS 1.3 with X25519 -- the field server sets no group or
			// cipher restrictions, so there is nothing to match on our side.
			"OpenSSL"
		});

		// FlatBuffers runtime headers, vendored. The engine only ships the
		// licence notice for FlatBuffers, not the headers, so there is nothing
		// to depend on instead. See ThirdParty/FlatBuffers/README.md.
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "ThirdParty", "FlatBuffers"));
		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
