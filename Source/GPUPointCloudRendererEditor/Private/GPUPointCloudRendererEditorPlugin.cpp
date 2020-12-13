/*************************************************************************************************
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, 2018
**************************************************************************************************/

#include "CoreMinimal.h"
#include "UObject/Class.h"
//#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "GPUPointCloudRendererComponent.h"
#include "IGPUPointCloudRendererEditorPlugin.h"


class FGPUPointCloudRendererEditorPlugin : public IGPUPointCloudRendererEditorPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		UE_LOG(GPUPointCloudRenderer, Log, TEXT("//////////////////////////////////////////// \n"));
		UE_LOG(GPUPointCloudRenderer, Log, TEXT("// Initializing GPU Point Cloud Renderer... \n"));
		UE_LOG(GPUPointCloudRenderer, Log, TEXT("//////////////////////////////////////////// \n"));

		#ifdef HAVE_CUDA
			UE_LOG(GPUPointCloudRenderer, Log, TEXT("Found CUDA installation. \n"));
		#endif
		#ifdef WITH_PCL
			UE_LOG(GPUPointCloudRenderer, Log, TEXT("Found PCL installation. \n"));
		#endif
	}


	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FGPUPointCloudRendererEditorPlugin, GPUPointCloudRendererEditor)
