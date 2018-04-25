/*************************************************************************************************
* Copyright (C) Valentin Kraft - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, February 2018
**************************************************************************************************/

#include "PointCloudRendererComponent.h"
#include "IPointCloudRenderer.h"
#include "PointCloudStreamingCore.h"
#include "PointCloudComponent.h"
#include "ConstructorHelpers.h"


DEFINE_LOG_CATEGORY(PointCloudRenderer);
DECLARE_CYCLE_STAT(TEXT("Update Shader Parameter"), STAT_UpdateShaderParameter, STATGROUP_PCR);
DECLARE_CYCLE_STAT(TEXT("Create Dynamic Base Mesh"), STAT_CreateDynamicBaseMesh, STATGROUP_PCR);

#define CHECK_PCR_STATUS																\
if (!IPointCloudRenderer::IsAvailable() /*|| !FPointCloudModule::IsAvailable()*/) {		\
	UE_LOG(PointCloudRenderer, Error, TEXT("Point Cloud Renderer module not loaded!"));	\
	return;																				\
}																						\
if (!mPointCloudCore) {																	\
	UE_LOG(PointCloudRenderer, Error, TEXT("Point Cloud Core component not found!"));	\
	return;																				\
}


UPointCloudRendererComponent::UPointCloudRendererComponent(const FObjectInitializer& ObjectInitializer)
{
	CHECK_PCR_STATUS

	/// Set default values
	PrimaryComponentTick.bCanEverTick = true;
	//this->GetOwner()->AutoReceiveInput = EAutoReceiveInput::Player0;

	ConstructorHelpers::FObjectFinder<UMaterial> MaterialRef(TEXT("Material'/PointCloudRenderer/Streaming/DynPCMat.DynPCMat'"));
	mStreamingBaseMat = MaterialRef.Object;
	mPointCloudMaterial = UMaterialInstanceDynamic::Create(mStreamingBaseMat, this->GetOwner());

	if (mPointCloudCore)
		delete mPointCloudCore;
	mPointCloudCore = IPointCloudRenderer::Get().CreateStreamingInstance(mPointCloudMaterial);
	//mPointCloudCore->currentWorld = GetWorld();
}

UPointCloudRendererComponent::~UPointCloudRendererComponent() {
	if (mPointCloudCore)
		delete mPointCloudCore;
}

//////////////////////
// MAIN FUNCTIONS ////
//////////////////////


void UPointCloudRendererComponent::FillPointCloudWithRandomPoints(int32 pointsPerAxis, float extent) {
	
	if (!(BaseMesh->NumPoints == pointsPerAxis * pointsPerAxis))
		CreateStreamingBaseMesh(pointsPerAxis*pointsPerAxis);

	if (mPointCloudCore)
		mPointCloudCore->FillPointCloudWithRandomPoints(pointsPerAxis, extent);

	mPointCount = pointsPerAxis * pointsPerAxis;
}

void UPointCloudRendererComponent::SetDynamicProperties(float cloudScaling, float falloff, float splatSize, float distanceScalingStart, float maxDistanceScaling, bool overrideColor) {
	
	mFalloff = falloff;
	mScaling = cloudScaling;
	mSplatSize = splatSize;
	mDistanceScalingStart = distanceScalingStart;
	mMaxDistanceScaling = maxDistanceScaling;
	mShouldOverrideColor = overrideColor;
}

void UPointCloudRendererComponent::SetDynamicPointStreamInput(TArray<FLinearColor> &pointPositions, TArray<FColor> &pointColors) {
	if (pointPositions.Num() != pointColors.Num()) {
		UE_LOG(PointCloudRenderer, Error, TEXT("The number of point positions doesn't match the number of point colors."));
		return;
	}
	if (!mPointCloudCore)
		return;

	CreateStreamingBaseMesh(pointPositions.Num());
	mPointCloudCore->SetCustomPointInput(pointPositions, pointColors);
	mShouldUpdateEveryFrame = true;
}


//////////////////////////
// STANDARD FUNCTIONS ////
//////////////////////////

//void UPointCloudRendererComponent::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) {
//	
//	Super::PostEditChangeProperty(PropertyChangedEvent);
//
//	CHECK_PCR_STATUS
//
//	UpdateSpriteComponentProperties();
//
//	SpriteComponent->UpdateBounds();
//	mExtent = (SpriteComponent->Bounds.GetBox().GetExtent()/50.0f).ToString();
//	PointCloudVisualisationHelper(mShowNormals, mShowEnclosingGrid);
//}

//void UPointCloudRendererComponent::PostEditComponentMove(bool bFinished) {
//	//PointCloudVisualisationHelper(mShowNormals, mShowEnclosingGrid);
//}


void UPointCloudRendererComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	// Update sprites if neccessary //#ToDo: better update function?
	//if (mShouldUpdateEveryFrame) 
	//	UpdateSprites();

	// Update core
	if (mPointCloudCore) {
		mPointCloudCore->Update(mShouldUpdateEveryFrame, mSortDataEveryFrame, DeltaTime);
		mPointCount = mPointCloudCore->GetPointCount();
	}

	// Update shader properties
	UpdateShaderProperties();
}


////////////////////////
// HELPER FUNCTIONS ////
////////////////////////


void UPointCloudRendererComponent::CreateStreamingBaseMesh(int32 pointCount)
{
	SCOPE_CYCLE_COUNTER(STAT_CreateDynamicBaseMesh);

	//Check if update is neccessary
	if (BaseMesh && BaseMesh->NumPoints == pointCount)
		return;

	// Create base mesh
	BaseMesh = NewObject<UPointCloudComponent>(this, FName("PointCloud Mesh"));
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


void UPointCloudRendererComponent::UpdateShaderProperties()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateShaderParameter);

	if (!mPointCloudMaterial)
		return;

	auto streamingMeshMatrix = this->GetComponentToWorld().ToMatrixWithScale();
	mPointCloudMaterial->SetVectorParameterValue("ObjTransformMatrixXAxis", streamingMeshMatrix.GetUnitAxis(EAxis::X));
	mPointCloudMaterial->SetVectorParameterValue("ObjTransformMatrixYAxis", streamingMeshMatrix.GetUnitAxis(EAxis::Y));
	mPointCloudMaterial->SetVectorParameterValue("ObjTransformMatrixZAxis", streamingMeshMatrix.GetUnitAxis(EAxis::Z));
	mPointCloudMaterial->SetVectorParameterValue("ObjScale", this->GetComponentScale() * mScaling);
	mPointCloudMaterial->SetScalarParameterValue("FalloffExpo", mFalloff);
	mPointCloudMaterial->SetScalarParameterValue("SplatSize", mSplatSize);
	mPointCloudMaterial->SetScalarParameterValue("ScalingStartDistance", mDistanceScalingStart);
	mPointCloudMaterial->SetScalarParameterValue("MaxDistanceScaling", mMaxDistanceScaling);
	mPointCloudMaterial->SetScalarParameterValue("ShouldOverrideColor", (int)mShouldOverrideColor);
}