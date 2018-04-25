/*************************************************************************************************
* Copyright (C) Valentin Kraft - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, February 2018
**************************************************************************************************/

#pragma once

#include "CoreMinimal.h"
//#include "ComputeShaderUsageExample.h"
//#include "PixelShaderUsageExample.h"

class POINTCLOUDRENDERER_API FPointCloudStreamingCore
{
public:
	FPointCloudStreamingCore(UMaterialInstanceDynamic* pointCloudShaderDynInstance = nullptr) { mDynamicMatInstance = pointCloudShaderDynInstance; };
	~FPointCloudStreamingCore();
	//virtual unsigned int GetInstanceId() const { return _instanceId; };
	unsigned int GetPointCount() { return mPointCount; };

	void FillPointCloudWithRandomPoints(int32 pointsPerAxis = 128, float extent = 100);
	void Update(bool isDynamicPointCloud, bool sortDataEveryFrame, float deltaTime);
	void UpdateDynamicMaterialForStreaming(UMaterialInstanceDynamic* pointCloudShaderDynInstance) { mDynamicMatInstance = pointCloudShaderDynInstance; };
	void SetCustomPointInput(TArray<FLinearColor> &pointPositions, TArray<FColor> &pointColors);

private:
	void InitializeStreaming(unsigned int pointCount);
	void UpdateStreamingTextures();
	void UpdateShaderParameter();
	void UpdateStreamingBuffers();
	void SortPointCloudData();
	void FreeData();

	class UMaterialInstanceDynamic* mDynamicMatInstance = nullptr;
	unsigned int mPointCount = 0;

	float TotalElapsedTime = 0.f;	//#Temp?

	// CPU buffers
	TArray<FLinearColor> mPointPosData;
	TArray<FLinearColor>* mPointPosDataPointer = nullptr;
	TArray<uint8> mColorData;
	TArray<FVector> mPointScalingData;

	// GPU texture buffers
	struct FUpdateTextureRegion2D* mUpdateTextureRegion = nullptr;
	class UTexture2D* mPointPosTexture = nullptr;
	UTexture2D* mPointScalingTexture = nullptr;
	UTexture2D* mColorTexture = nullptr;
	
};

