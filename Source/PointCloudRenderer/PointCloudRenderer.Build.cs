// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.IO;
using System;

namespace UnrealBuildTool.Rules
{
    public class PointCloudRenderer : ModuleRules
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

        public PointCloudRenderer(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateIncludePaths.Add("PointCloudRenderer/Private");
            PublicIncludePaths.Add("PointCloudRenderer/Public");

            PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "RHI", "ComputeShader", "PixelShader" });
            PrivateDependencyModuleNames.AddRange(new string[] { "Core", "Projects" });

        }
    }
}
