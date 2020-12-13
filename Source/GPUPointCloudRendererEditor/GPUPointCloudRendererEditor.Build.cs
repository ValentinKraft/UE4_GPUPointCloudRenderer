
using UnrealBuildTool;
using System.IO;
using System;

public class GPUPointCloudRendererEditor : ModuleRules
{
	public GPUPointCloudRendererEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        //Type = ModuleType.Editor;

        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

        PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    //"PropertyEditor",
                    //"BlueprintGraph",
                    "GPUPointCloudRenderer",
                    //"ShaderCore",
                    "RenderCore",
                    "CustomMeshComponent",
                    "RHI"
                }
            );

        //if (UEBuildConfiguration.bBuildEditor)
        //    PublicDependencyModuleNames.Add("UnrealEd");

        //PrivateDependencyModuleNames.AddRange(
        //    new string[]
        //    {
        //            "EditorStyle",
        //            "AssetRegistry"
        //    }
        //);

        // DynamicallyLoadedModuleNames.AddRange(new string[] { "GPUPointCloudRenderer" });

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
