/*************************************************************************************************
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, 2018
**************************************************************************************************/

#include "PointCloudStreamingCore.h"
#include "IGPUPointCloudRenderer.h"
#include "Modules/ModuleManager.h"


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
	FPointCloudStreamingCore* CreateStreamingInstance(UMaterialInstanceDynamic* pointCloudShaderDynInstance) override {
		return new FPointCloudStreamingCore(pointCloudShaderDynInstance);
	}
};

IMPLEMENT_MODULE(FGPUPointCloudRendererPlugin, GPUPointCloudRenderer)
