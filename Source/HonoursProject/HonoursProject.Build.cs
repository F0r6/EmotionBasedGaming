// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class HonoursProject : ModuleRules
{
	public HonoursProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"OpenCV",
			"OpenCVHelper",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"MediaIOCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "Media", "MediaIOCore" });

		PublicIncludePaths.AddRange(new string[] {
			"HonoursProject",
			"HonoursProject/Variant_Horror",
			"HonoursProject/Variant_Horror/UI",
			"HonoursProject/Variant_Shooter",
			"HonoursProject/Variant_Shooter/AI",
			"HonoursProject/Variant_Shooter/UI",
			"HonoursProject/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
