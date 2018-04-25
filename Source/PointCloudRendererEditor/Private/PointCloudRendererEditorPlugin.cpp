/*************************************************************************************************
* Copyright (C) Valentin Kraft - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, February 2018
**************************************************************************************************/

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "PointCloudRendererComponent.h"
#include "IPointCloudRendererEditorPlugin.h"


class FPointCloudRendererEditorPlugin : public IPointCloudRendererEditorPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		UE_LOG(PointCloudRenderer, Log, TEXT("/////////////////////////////////////// \n"));
		UE_LOG(PointCloudRenderer, Log, TEXT("// Initializing Point Cloud Renderer... \n"));
		UE_LOG(PointCloudRenderer, Log, TEXT("/////////////////////////////////////// \n"));

		#if defined HAVE_CUDA
			UE_LOG(PointCloudRenderer, Log, TEXT("Found CUDA installation. \n"));
		#endif
	}


	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FPointCloudRendererEditorPlugin, PointCloudRendererEditor)
