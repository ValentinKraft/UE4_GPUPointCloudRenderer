/*************************************************************************************************
* Copyright (C) Valentin Kraft - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, February 2018
**************************************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "GPUPointCloudRendererComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(GPUPointCloudRenderer, Log, All);

UCLASS( ClassGroup=Rendering, meta=(BlueprintSpawnableComponent), hideCategories = (Object, LOD, Physics, Collision))
class GPUPOINTCLOUDRENDEREREDITOR_API UGPUPointCloudRendererComponent : public USceneComponent
{
	GENERATED_BODY()

public:	
	UGPUPointCloudRendererComponent(const FObjectInitializer& ObjectInitializer);
	~UGPUPointCloudRendererComponent();

	UPROPERTY()
	class UPointCloudComponent* BaseMesh;

	/**
	* For dynamic point clouds only. When you want to change properties, then you'll have to call this function (during Run-Time or in construction script). Sets the given point cloud properties and updates the point cloud. Can be called every frame.
	*
	* @param	cloudScaling				The scaling factor of the whole point cloud. Scaling can be also changed by the component scaling in the editor, but not during Run-Time.
	* @param	falloff						The softness of the point's edge.
	* @param	splatSize					The splat size.
	* @param	distanceScalingStart		The distance from the camera, where the scaling of the points start to increase.
	* @param	maxDistanceScaling			Maximal scaling value (set to 1 if you want to disable scaling by distance).
	* @param	overrideColor				Overrides the point cloud colors with the given color.
	*/
	UFUNCTION(DisplayName = "PCR Set Dynamic Point Cloud Properties", BlueprintCallable, Category = "GPUPointCloudRenderer", meta = (Keywords = "point cloud update set properties"))
	void SetDynamicProperties(float cloudScaling = 1.0f, float falloff = 1.0f, float splatSize = 1.0f, float distanceScalingStart = 1000.f, float maxDistanceScaling = 3.f, bool overrideColor = false);

	/**
	* Send your own, custom point data stream to the renderer. Could be used for a kinect point stream or similar. Can be called every frame. Point positions have to be encoded as a array of LinearColors with the following mapping:
		Alpha Channel : Z-values;
		Green Channel : X-values;
		Blue Channel : Y-values;
		Red Channel : Z-values;
	*
	* @param	pointPositions				Array of your point positions. Please mind the mapping: Alpha Channel : Z-values, Green Channel : X-values, Blue Channel : Y-values, Red Channel : Z-values.
	* @param	pointColors					Array of your point colors.
	*/
	UFUNCTION(DisplayName = "PCR Set/Stream Input (FLinearColor/FColor)", BlueprintCallable, Category = "GPUPointCloudRenderer", meta = (Keywords = "set kinect custom own dynamic point cloud streaming input"))
	void SetInputAndConvert1(UPARAM(ref) TArray<FLinearColor> &pointPositions, UPARAM(ref) TArray<FColor> &pointColors, bool sortDataEveryFrame = false);

	/**
	* Send your own, custom point data stream to the renderer. Could be used for a kinect point stream or similar. Can be called every frame. Point positions have to be encoded as a array of LinearColors with the following mapping:
	Alpha Channel : Z-values;
	Green Channel : X-values;
	Blue Channel : Y-values;
	Red Channel : Z-values;
	*
	* @param	pointPositions				Array of your point positions. Please mind the mapping: Alpha Channel : Z-values, Green Channel : X-values, Blue Channel : Y-values, Red Channel : Z-values.
	* @param	pointColors					Array of your point colors.
	*/
	UFUNCTION(DisplayName = "PCR Set/Stream Input FAST", BlueprintCallable, Category = "GPUPointCloudRenderer", meta = (Keywords = "set kinect custom own dynamic point cloud streaming input"))
	void SetInput(UPARAM(ref) TArray<FLinearColor> &pointPositions, UPARAM(ref) TArray<uint8> &pointColors, bool sortDataEveryFrame = false);

	/**
	* Send your own, custom point data stream to the renderer. Could be used for a kinect point stream or similar. Can be called every frame.
	*
	* @param	pointPositions				Array of your point positions.
	* @param	pointColors					Array of your point colors.
	*/
	UFUNCTION(DisplayName = "PCR Set/Stream Input (FVector/FColor)", BlueprintCallable, Category = "GPUPointCloudRenderer", meta = (Keywords = "set kinect custom own dynamic point cloud streaming input"))
	void SetInputAndConvert2(UPARAM(ref) TArray<FVector> &pointPositions, UPARAM(ref) TArray<FColor> &pointColors, bool sortDataEveryFrame = false);

	/**
	* Sets the extents of the point cloud. Needed for proper coloring with a gradient.
	*
	* @param	extent						The extent of the cloud.
	*/
	UFUNCTION(DisplayName = "PCR Set Extent", BlueprintCallable, Category = "GPUPointCloudRenderer", meta = (Keywords = "set extent point cloud"))
	void SetExtent(FBox extent);

private:
	class FPointCloudStreamingCore* mPointCloudCore = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "GPUPointCloudRenderer")
	int32 mPointCount = 0;
	UPROPERTY(VisibleAnywhere, Category = "GPUPointCloudRenderer")
	FString mExtent = "No data.";
	UPROPERTY(VisibleAnywhere, Category = "GPUPointCloudRenderer")
	UMaterialInstanceDynamic* mPointCloudMaterial = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "GPUPointCloudRenderer")
	float mSplatSize = 1.0f;
	UPROPERTY(VisibleAnywhere, Category = "GPUPointCloudRenderer")
	float mPointCloudScaling = 1.0f;

	/// Streaming-specific variables
	UPROPERTY()
	UMaterialInterface* mStreamingBaseMat = nullptr;
	float mFalloff = 2.0f;
	float mScaling = 1.0f;
	float mDistanceScalingStart = 1000.f;
	float mMaxDistanceScaling = 3.f;
	bool mShouldOverrideColor = false;

	void CreateStreamingBaseMesh(int32 pointCount = 1);
	void UpdateShaderProperties();
	//void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent);

public:	
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//void PostEditComponentMove(bool bFinished) override;

};