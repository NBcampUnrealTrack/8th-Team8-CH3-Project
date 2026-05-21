// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Oblivio : ModuleRules
{
	public Oblivio(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// NavigationSystem pulls UnrealEd as a public dependency in editor builds, which otherwise makes Oblivio
		// pick the UnrealEd shared PCH and reference editor-only globals (Slate/CoreStyleConstants, EditorViewportDefs, …)
		// that are not linked into UnrealEditor-Oblivio.dll — LNK2001. A gameplay private PCH avoids that mismatch.
		PrivatePCHHeaderFile = "OblivioPrivatePCH.h";

		// NavigationSystem pulls UnrealEd as a public dependency in editor builds, which otherwise makes Oblivio
		// pick the UnrealEd shared PCH and reference editor-only globals (Slate/CoreStyleConstants, EditorViewportDefs, …)
		// that are not linked into UnrealEditor-Oblivio.dll — LNK2001. A gameplay private PCH avoids that mismatch.
		PrivatePCHHeaderFile = "OblivioPrivatePCH.h";
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule", "Niagara", "UMG", "Slate", "SlateCore" });

		PrivateDependencyModuleNames.AddRange(new string[] { "NavigationSystem", "LevelSequence", "MovieScene" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
