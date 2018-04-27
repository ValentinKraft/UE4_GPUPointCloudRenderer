/*************************************************************************************************
* Copyright (C) Valentin Kraft - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, February 2018
**************************************************************************************************/

#include "CoreMinimal.h"
#include "PointCloudStreamingCore.h"
#include "Modules/ModuleManager.h"
#include "IGPUPointCloudRenderer.h"

class FGPUPointCloudRendererPlugin : public IGPUPointCloudRenderer
{

	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}

	/**
	* Returns a instance of the Point Cloud Core class.
	*/
	FPointCloudStreamingCore* CreateStreamingInstance(UMaterialInstanceDynamic* pointCloudShaderDynInstance) {
		return new FPointCloudStreamingCore(pointCloudShaderDynInstance);
	}
};

IMPLEMENT_MODULE(FGPUPointCloudRendererPlugin, GPUPointCloudRenderer)
