// Copyright LOST ISLAND Team. All Rights Reserved.

using UnrealBuildTool;

public class LostIslandEditorTarget : TargetRules
{
	public LostIslandEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LostIsland");
	}
}
