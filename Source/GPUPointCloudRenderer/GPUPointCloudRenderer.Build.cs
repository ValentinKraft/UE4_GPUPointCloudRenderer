// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.IO;
using System;

namespace UnrealBuildTool.Rules
{
    public class GPUPointCloudRenderer : ModuleRules
    {
        private string ModulePath
        {
            get { return ModuleDirectory; }
        }

        private string ThirdPartyPath
        {
            get { return Path.GetFullPath(Path.Combine(ModulePath, "../ThirdParty/")); }
        }

        private string BinariesPath
        {
            get { return Path.GetFullPath(Path.Combine(ModulePath, "../../Binaries/")); }
        }

        public GPUPointCloudRenderer(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateIncludePaths.Add("GPUPointCloudRenderer/Private");
            PublicIncludePaths.Add("GPUPointCloudRenderer/Public");

            PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "RHI", "ComputeShader", "PixelShader" });
            PrivateDependencyModuleNames.AddRange(new string[] { "Core", "Projects" });

        }
    }
}
