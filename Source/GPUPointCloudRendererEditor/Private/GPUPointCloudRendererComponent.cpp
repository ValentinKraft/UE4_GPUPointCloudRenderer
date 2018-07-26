/*************************************************************************************************
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, 2018
**************************************************************************************************/

#include "GPUPointCloudRendererComponent.h"
#include "IGPUPointCloudRenderer.h"
#include "PointCloudStreamingCore.h"
#include "PointCloudMeshBuilder.h"
#include "ConstructorHelpers.h"


DEFINE_LOG_CATEGORY(GPUPointCloudRenderer);

#define CHECK_PCR_STATUS																\
if (!IGPUPointCloudRenderer::IsAvailable() /*|| !FPointCloudModule::IsAvailable()*/) {		\
	UE_LOG(GPUPointCloudRenderer, Error, TEXT("Point Cloud Renderer module not loaded!"));	\
	return;																				\
}																						\
if (!mPointCloudCore) {																	\
	UE_LOG(GPUPointCloudRenderer, Error, TEXT("Point Cloud Core component not found!"));	\
	return;																				\
}


UGPUPointCloudRendererComponent::UGPUPointCloudRendererComponent(const FObjectInitializer& ObjectInitializer)
{
	/// Set default values
	PrimaryComponentTick.bCanEverTick = true;
	//this->GetOwner()->AutoReceiveInput = EAutoReceiveInput::Player0;

	ConstructorHelpers::FObjectFinder<UMaterial> MaterialRef(TEXT("Material'/GPUPointCloudRenderer/Streaming/DynPCMat.DynPCMat'"));
	mStreamingBaseMat = MaterialRef.Object;
	mPointCloudMaterial = UMaterialInstanceDynamic::Create(mStreamingBaseMat, this->GetOwner());

	if (mPointCloudCore)
		delete mPointCloudCore;
	mPointCloudCore = IGPUPointCloudRenderer::Get().CreateStreamingInstance(mPointCloudMaterial);
}

UGPUPointCloudRendererComponent::~UGPUPointCloudRendererComponent() {
	if (mPointCloudCore)
		delete mPointCloudCore;
}

//////////////////////
// MAIN FUNCTIONS ////
//////////////////////


void UGPUPointCloudRendererComponent::SetDynamicProperties(float cloudScaling, float falloff, float splatSize, float distanceScaling, float distanceFalloff, bool overrideColor) {
	
	mFalloff = falloff;
	mScaling = cloudScaling;
	mSplatSize = splatSize;
	mDistanceScaling = distanceScaling;
	mDistanceFalloff = distanceFalloff;
	mShouldOverrideColor = overrideColor;
}

void UGPUPointCloudRendererComponent::SetInputAndConvert1(TArray<FLinearColor> &pointPositions, TArray<FColor> &pointColors, bool sortData) {
	
	CHECK_PCR_STATUS

	if (pointPositions.Num() != pointColors.Num())
		UE_LOG(GPUPointCloudRenderer, Warning, TEXT("The number of point positions doesn't match the number of point colors."));
	if (pointPositions.Num() == 0 || pointColors.Num() == 0) {
		UE_LOG(GPUPointCloudRenderer, Error, TEXT("Empty point position and/or color data."));
		return;
	}

	CreateStreamingBaseMesh(pointPositions.Num());
	mPointCloudCore->SetInput(pointPositions, pointColors, sortData);
}

void UGPUPointCloudRendererComponent::AddSnapshot(TArray<FLinearColor> &pointPositions, TArray<uint8> &pointColors, FVector offsetTranslation, FRotator offsetRotation) {
	
	CHECK_PCR_STATUS

	if (pointPositions.Num() * 4 != pointColors.Num())
		UE_LOG(GPUPointCloudRenderer, Warning, TEXT("The number of point positions doesn't match the number of point colors."));
	if (pointPositions.Num() == 0 || pointColors.Num() == 0) {
		UE_LOG(GPUPointCloudRenderer, Error, TEXT("Empty point position and/or color data."));
		return;
	}

	CreateStreamingBaseMesh(MAXTEXRES * MAXTEXRES);

	// Since the point is later transformed to the local coordinate system, we have to inverse transform it beforehand
	FMatrix objMatrix = this->GetComponentToWorld().ToMatrixWithScale();
	offsetTranslation = objMatrix.InverseTransformVector(offsetTranslation);

	mPointCloudCore->AddSnapshot(pointPositions, pointColors, offsetTranslation, offsetRotation);
}

void UGPUPointCloudRendererComponent::SetInput(TArray<FLinearColor> &pointPositions, TArray<uint8> &pointColors, bool sortData) {
	
	CHECK_PCR_STATUS

	if (pointPositions.Num()*4 != pointColors.Num())
		UE_LOG(GPUPointCloudRenderer, Warning, TEXT("The number of point positions doesn't match the number of point colors."));
	if (pointPositions.Num() == 0 || pointColors.Num() == 0) {
		UE_LOG(GPUPointCloudRenderer, Error, TEXT("Empty point position and/or color data."));
		return;
	}

	CreateStreamingBaseMesh(pointPositions.Num());
	mPointCloudCore->SetInput(pointPositions, pointColors, sortData);
}

void UGPUPointCloudRendererComponent::SetInputAndConvert2(TArray<FVector> &pointPositions, TArray<FColor> &pointColors, bool sortData) {
	
	CHECK_PCR_STATUS

	if (pointPositions.Num() != pointColors.Num())
		UE_LOG(GPUPointCloudRenderer, Warning, TEXT("The number of point positions doesn't match the number of point colors."));
	if (pointPositions.Num() == 0 || pointColors.Num() == 0) {
		UE_LOG(GPUPointCloudRenderer, Error, TEXT("Empty point position and/or color data."));
		return;
	}

	CreateStreamingBaseMesh(pointPositions.Num());
	mPointCloudCore->SetInput(pointPositions, pointColors, sortData);
}

void UGPUPointCloudRendererComponent::SetExtent(FBox extent) {
	
	CHECK_PCR_STATUS

	mPointCloudCore->SetExtent(extent);
	mExtent = extent.ToString();
}

//////////////////////////
// STANDARD FUNCTIONS ////
//////////////////////////


void UGPUPointCloudRendererComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	// Update sprites if neccessary //#ToDo: better update function?
	//if (mShouldUpdateEveryFrame) 
	//	UpdateSprites();

	// Update core
	if (mPointCloudCore) {
		mPointCloudCore->Update(DeltaTime);
		mPointCount = mPointCloudCore->GetPointCount();
	}

	// Update shader properties
	UpdateShaderProperties();
}

void UGPUPointCloudRendererComponent::BeginPlay() {
	Super::BeginPlay();

}


////////////////////////
// HELPER FUNCTIONS ////
////////////////////////


void UGPUPointCloudRendererComponent::CreateStreamingBaseMesh(int32 pointCount)
{
	CHECK_PCR_STATUS
	//SCOPE_CYCLE_COUNTER(STAT_CreateDynamicBaseMesh);

	//Check if update is neccessary
	if (BaseMesh && BaseMesh->NumPoints == pointCount)
		return;
	if (pointCount == 0)
		return;

	// Create base mesh
	BaseMesh = NewObject<UPointCloudMeshBuilder>(this, FName("PointCloud Mesh"));
	BaseMesh->NumPoints = pointCount;
	BaseMesh->triangleSize = 1.0f;	// splat size is set in the shader
	BaseMesh->RegisterComponent();
	BaseMesh->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	BaseMesh->SetMaterial(0, mStreamingBaseMat);
	BaseMesh->SetAbsolute(false, true, true);	// Disable scaling for the mesh - the scaling vector is transferred via a shader parameter in UpdateShaderProperties()

	// Update material
	mPointCloudMaterial = BaseMesh->CreateAndSetMaterialInstanceDynamic(0);
	mPointCloudCore->UpdateDynamicMaterialForStreaming(mPointCloudMaterial);
}


void UGPUPointCloudRendererComponent::UpdateShaderProperties()
{
	//SCOPE_CYCLE_COUNTER(STAT_UpdateShaderParameter);

	if (!mPointCloudMaterial)
		return;

	auto streamingMeshMatrix = this->GetComponentToWorld().ToMatrixWithScale();
	mPointCloudMaterial->SetVectorParameterValue("ObjTransformMatrixXAxis", streamingMeshMatrix.GetUnitAxis(EAxis::X));
	mPointCloudMaterial->SetVectorParameterValue("ObjTransformMatrixYAxis", streamingMeshMatrix.GetUnitAxis(EAxis::Y));
	mPointCloudMaterial->SetVectorParameterValue("ObjTransformMatrixZAxis", streamingMeshMatrix.GetUnitAxis(EAxis::Z));
	mPointCloudMaterial->SetVectorParameterValue("ObjScale", this->GetComponentScale() * mScaling);
	mPointCloudMaterial->SetScalarParameterValue("FalloffExpo", mFalloff);
	mPointCloudMaterial->SetScalarParameterValue("SplatSize", mSplatSize);
	mPointCloudMaterial->SetScalarParameterValue("DistanceScaling", mDistanceScaling);
	mPointCloudMaterial->SetScalarParameterValue("DistanceFalloff", mDistanceFalloff);
	mPointCloudMaterial->SetScalarParameterValue("ShouldOverrideColor", (int)mShouldOverrideColor);
}