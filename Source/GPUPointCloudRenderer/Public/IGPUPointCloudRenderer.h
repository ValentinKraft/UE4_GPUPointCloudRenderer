/*************************************************************************************************
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, 2018
**************************************************************************************************/

#pragma once

#include "CoreMinimal.h"
//#include "Materials/MaterialInstanceDynamic.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class UMaterialInstanceDynamic;
class FPointCloudStreamingCore;

/**
 * The public interface to the Point Cloud Renderer module.
 * @Author Valentin Kraft
 */
class GPUPOINTCLOUDRENDERER_API IGPUPointCloudRenderer : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IGPUPointCloudRenderer& Get()
	{
		return FModuleManager::LoadModuleChecked< IGPUPointCloudRenderer >( "GPUPointCloudRenderer" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "GPUPointCloudRenderer" );
	}

	/**
	* Returns a instance of the Point Cloud Streaming Core class.
	*/
	virtual FPointCloudStreamingCore* CreateStreamingInstance(UMaterialInstanceDynamic* pointCloudShaderDynInstance = nullptr) = 0;

};

