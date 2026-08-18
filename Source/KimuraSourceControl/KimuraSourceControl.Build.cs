// Copyright Kimura Software Inc.

using System.IO;
using UnrealBuildTool;

public class KimuraSourceControl : ModuleRules
{
	public KimuraSourceControl(ReadOnlyTargetRules Target) : base(Target)
	{

		bUseUnity = false; 

		this.OptimizeCode = CodeOptimization.Never;

		this.PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"InputCore",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"SourceControl",
				"UnrealEd",
				"DesktopWidgets",
				"Engine",
				"LevelEditor",
				"JsonUtilities",
				"Json",
				"Projects",
				"SceneOutliner",
		});

		bool bVerbose = false;
		PrivateDefinitions.Add($"KIMURA_VERBOSE={(bVerbose ? 1 : 0)}");

		bool bRecordHostCommands = false;
		PrivateDefinitions.Add($"KIMURA_RECORD_HOST_COMMANDS={(bRecordHostCommands ? 1 : 0)}");
	}
}
