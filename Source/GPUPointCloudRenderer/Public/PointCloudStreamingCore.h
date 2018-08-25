/*************************************************************************************************
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, 2018
**************************************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "ComputeShaderUsageExample.h"
#include "Runtime/Engine/Classes/Engine/Texture2D.h"

#define PCR_MAXTEXRES 2048
#define PCR_MAX_SORT_COUNT NUM_ELEMENTS

DECLARE_STATS_GROUP(TEXT("GPUPointCloudRenderer"), STATGROUP_GPUPCR, STATCAT_Advanced);

class GPUPOINTCLOUDRENDERER_API FPointCloudStreamingCore
{
public:
	FPointCloudStreamingCore(UMaterialInstanceDynamic* pointCloudShaderDynInstance = nullptr) { mDynamicMatInstance = pointCloudShaderDynInstance; };
	~FPointCloudStreamingCore();
	//virtual unsigned int GetInstanceId() const { return _instanceId; };
	unsigned int GetPointCount() { return mPointCount; };

	void Update(float deltaTime) { UpdateShaderParameter();	mDeltaTime += deltaTime; };
	void UpdateDynamicMaterialForStreaming(UMaterialInstanceDynamic* pointCloudShaderDynInstance) { mDynamicMatInstance = pointCloudShaderDynInstance; };
	bool SetInput(TArray<FLinearColor> &pointPositions, TArray<uint8> &pointColors, bool sortDataEveryFrame = false);
	bool SetInput(TArray<FLinearColor> &pointPositions, TArray<FColor> &pointColors, bool sortDataEveryFrame = false);
	bool SetInput(TArray<FVector> &pointPositions, TArray<FColor> &pointColors, bool sortDataEveryFrame = false);
	void SetExtent(FBox extent) { mExtent = extent; };
	void AddSnapshot(TArray<FLinearColor> &pointPositions, TArray<uint8> &pointColors, FVector offsetTranslation = FVector::ZeroVector, FRotator offsetRotation = FRotator::ZeroRotator);
	void SavePointPosDataToTexture(UTextureRenderTarget2D* pointPosRT);
	void SaveColorDataToTexture(UTextureRenderTarget2D* colorsRT);
	bool SortPointCloudData();

	UWorld* currentWorld = nullptr;
	// The current camera position ! in object space ! of the point cloud proxy mesh.
	FVector currentCamPos = FVector::ZeroVector;

	float mStreamCaptureSteps = 0.5f;
	unsigned int mGlobalStreamCounter = 0;

private:
	void Initialize(unsigned int pointCount);
	void InitColorBuffer();
	void InitPointPosBuffer();
	bool UpdateTextureBuffer();
	void UpdateShaderParameter();
	void FreeData();
	unsigned int GetUpperPowerOfTwo(unsigned int v);

	class UMaterialInstanceDynamic* mDynamicMatInstance = nullptr;
	unsigned int mPointCount = 0;
	
	FBox mExtent;
	float mDeltaTime = 10.f;
	bool mWasSorted = false;

	// CPU buffers
	TArray<FLinearColor> mPointPosData;
	TArray<FLinearColor>* mPointPosDataPointer = &mPointPosData;
	TArray<uint8> mColorData;
	TArray<uint8>* mColorDataPointer = &mColorData;
	TArray<FVector> mPointScalingData;

	// GPU texture buffers
	struct FUpdateTextureRegion2D* mUpdateTextureRegion = nullptr;
	UTexture2D* mPointPosTexture = nullptr;
	UTexture2D* mPointScalingTexture = nullptr;
	UTexture2D* mColorTexture = nullptr;
	
	// Sorting-related variables
	class FComputeShaderUsageExample* ComputeShading = nullptr;
	class FPixelShaderUsageExample* PixelShading = nullptr;
	class UTextureRenderTarget2D* RenderTarget = nullptr;
	UTexture* CastedRenderTarget = nullptr;
};

