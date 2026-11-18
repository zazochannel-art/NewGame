// Copyright LOST ISLAND Team. All Rights Reserved.

using UnrealBuildTool;

public class LostIslandTarget : TargetRules
{
	public LostIslandTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LostIsland");
	}
}
