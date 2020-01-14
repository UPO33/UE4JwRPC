// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class JwRPC : ModuleRules
{
	public JwRPC(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        //bFasterWithoutUnity = true;
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
            });
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "CoreUObject",
                "Engine",
                "Core",
                "WebSockets",
                "Json",
                "JsonUtilities",
                "JsonBP",
				// ... add other public dependencies that you statically link with here ...
			});
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{

				
                
				// ... add private dependencies that you statically link with here ...	
			});
    }
}
