/*************************************************************************************************
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, 2018
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
	class UPointCloudMeshBuilder* BaseMesh;

	/**
	* For dynamic point clouds only. When you want to change properties, then you'll have to call this function (during Run-Time or in construction script). Sets the given point cloud properties and updates the point cloud. Can be called every frame.
	*
	* @param	cloudScaling				The scaling factor of the whole point cloud. Scaling can be also changed by the component scaling in the editor, but not during Run-Time.
	* @param	falloff						The softness of the point's edge.
	* @param	splatSize					The splat size.
	* @param	distanceScaling				The distance from the camera, where the scaling of the points start to increase.
	* @param	distanceFalloff				The scaling falloff depending on the distance to the camera (1=linear, 2=quadratic).
	* @param	overrideColor				Overrides the point cloud colors with the given colormap.
	*/
	UFUNCTION(DisplayName = "PCR Set Dynamic Point Cloud Properties", BlueprintCallable, Category = "GPUPointCloudRenderer", meta = (Keywords = "point cloud update set properties"))
	void SetDynamicProperties(float cloudScaling = 1.0f, float falloff = 1.0f, float splatSize = 1.0f, float distanceScaling = 1000.f, float distanceFalloff = 1.1f, bool overrideColor = false);

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
	* @param	pointColors					Array of your point colors (BGRA-encoded).
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

	/**
	* Creates a large datasets and adds the given data as a "snapshot" to it. Can be used to collect different point cloud datasets into one large set (e.g. collecting a 360°-View with several captures of the environment etc.).
	*
	* @param	pointPositions				Array of your point positions.
	* @param	pointColors					Array of your point colors (BGRA-encoded).
	* @param	offsetTranslation			The World-Space offset translation the given point cloud should be saved with.
	* @param	offsetRotation				The World-Space offset rotation the given point cloud should be saved with.
	*/
	UFUNCTION(DisplayName = "PCR Add Point Cloud Snapshot", BlueprintCallable, Category = "GPUPointCloudRenderer", meta = (Keywords = "set add input increment point cloud collect snapshot kinect"))
	void AddSnapshot(UPARAM(ref) TArray<FLinearColor> &pointPositions, UPARAM(ref) TArray<uint8> &pointColors, FVector offsetTranslation = FVector::ZeroVector, FRotator offsetRotation = FRotator::ZeroRotator);

private:
	class FPointCloudStreamingCore* mPointCloudCore = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "GPUPointCloudRenderer")
	int32 mPointCount = 0;
	UPROPERTY(VisibleAnywhere, Category = "GPUPointCloudRenderer")
	FString mExtent = "No data.";
	UPROPERTY(VisibleAnywhere, Category = "GPUPointCloudRenderer")
	class UMaterialInstanceDynamic* mPointCloudMaterial = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "GPUPointCloudRenderer")
	float mSplatSize = 1.0f;
	UPROPERTY(VisibleAnywhere, Category = "GPUPointCloudRenderer")
	float mPointCloudScaling = 1.0f;

	/// Streaming-specific variables
	UPROPERTY()
	UMaterialInterface* mStreamingBaseMat = nullptr;
	float mFalloff = 2.0f;
	float mScaling = 1.0f;
	float mDistanceScaling = 1000.f;
	float mDistanceFalloff = 3.f;
	bool mShouldOverrideColor = false;

	void CreateStreamingBaseMesh(int32 pointCount = 1);
	void UpdateShaderProperties();
	//void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent);

public:	
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void BeginPlay() override;
	//void PostEditComponentMove(bool bFinished) override;

};