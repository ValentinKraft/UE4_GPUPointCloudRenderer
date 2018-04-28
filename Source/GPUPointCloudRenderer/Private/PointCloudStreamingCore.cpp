/*************************************************************************************************
* Copyright (C) Valentin Kraft - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, February 2018
**************************************************************************************************/

#include "CoreMinimal.h"
#include "PointCloudStreamingCore.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "App.h"
//#include "ComputeShaderUsageExample.h"
//#include "PixelShaderUsageExample.h"

using namespace std;
#undef UpdateResource

DECLARE_CYCLE_STAT(TEXT("Generate Random Cloud"), STAT_GenerateRandomCloud, STATGROUP_GPUPCR);
DECLARE_CYCLE_STAT(TEXT("Fill CPU Buffer"), STAT_FillCPUBuffer, STATGROUP_GPUPCR);
DECLARE_CYCLE_STAT(TEXT("Update Texture Regions"), STAT_UpdateTextureRegions, STATGROUP_GPUPCR);
DECLARE_CYCLE_STAT(TEXT("Sort Point Cloud Data"), STAT_SortPointCloudData, STATGROUP_GPUPCR);
DECLARE_CYCLE_STAT(TEXT("Update Shader Textures"), STAT_UpdateShaderTextures, STATGROUP_GPUPCR);


//////////////////////
// MAIN FUNCTIONS ////
//////////////////////


void FPointCloudStreamingCore::InitializeStreaming(unsigned int pointCount)
{
	int32 pointsPerAxis = FMath::CeilToInt(FMath::Sqrt(pointCount));
	mPointCount = pointsPerAxis * pointsPerAxis;

	// Check if update is neccessary
	if (mColorData.Num() == mPointCount * 4 && mPointPosDataPointer->Num() == mPointCount && mPointPosTexture && mColorTexture && mPointScalingTexture)
		return;

	// create point cloud positions texture
	mPointPosTexture = UTexture2D::CreateTransient(pointsPerAxis, pointsPerAxis, EPixelFormat::PF_A32B32G32R32F);
	mPointPosTexture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
	mPointPosTexture->SRGB = 0;
	mPointPosTexture->AddToRoot();
	mPointPosTexture->UpdateResource();
	mPointPosTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;

	// create point cloud scalings texture
	mPointScalingTexture = UTexture2D::CreateTransient(pointsPerAxis, pointsPerAxis, EPixelFormat::PF_A32B32G32R32F);
	mPointScalingTexture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
	mPointScalingTexture->SRGB = 0;
	mPointScalingTexture->AddToRoot();
	mPointScalingTexture->UpdateResource();
	mPointScalingTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;

	// create color texture
	mColorTexture = UTexture2D::CreateTransient(pointsPerAxis, pointsPerAxis, EPixelFormat::PF_B8G8R8A8);
	mColorTexture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
	mColorTexture->SRGB = 1;
	mColorTexture->AddToRoot();
	mColorTexture->UpdateResource();
	mColorTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;

	FreeData();

	mPointScalingData.Init(FVector::OneVector, mPointCount);
	mUpdateTextureRegion = new FUpdateTextureRegion2D(0, 0, 0, 0, pointsPerAxis, pointsPerAxis);

}

void FPointCloudStreamingCore::FreeData()
{
	mPointPosData.Empty();
	mPointPosDataPointer = nullptr;
	mColorData.Empty();
	mPointScalingData.Empty();
	if (mUpdateTextureRegion) delete mUpdateTextureRegion; mUpdateTextureRegion = nullptr;
}

void FPointCloudStreamingCore::SetInput(TArray<FLinearColor> &pointPositions, TArray<uint8> &pointColors, bool sortDataEveryFrame) {

	ensure(pointPositions.Num() == pointColors.Num());
	InitializeStreaming(pointPositions.Num());

	mPointPosDataPointer = &pointPositions;
	mColorDataPointer = &pointColors;

	if (sortDataEveryFrame)
		SortPointCloudData();
	UpdateStreamingTextures();
}

void FPointCloudStreamingCore::SetInput(TArray<FLinearColor> &pointPositions, TArray<FColor> &pointColors, bool sortDataEveryFrame) {
	
	ensure(pointPositions.Num() == pointColors.Num());
	InitializeStreaming(pointPositions.Num());	

	mPointPosDataPointer = &pointPositions;

	if (mColorData.Num() < pointColors.Num()) {
		mColorData.Empty();
		mColorData.AddUninitialized(mPointCount * 4); // 4 as we have bgra
	}

	for (int i = 0; i < pointColors.Num(); ++i) {		

		mColorData[i * 4] = pointColors[i].R;
		mColorData[i * 4 + 1] = pointColors[i].G;
		mColorData[i * 4 + 2] = pointColors[i].B;
		mColorData[i * 4 + 3] = pointColors[i].A;
	}

	mColorDataPointer = &mColorData;

	if (sortDataEveryFrame)
		SortPointCloudData();
	UpdateStreamingTextures();
}

void FPointCloudStreamingCore::SetInput(TArray<FVector> &pointPositions, TArray<FColor> &pointColors, bool sortDataEveryFrame) {

	ensure(pointPositions.Num() == pointColors.Num());
	InitializeStreaming(pointPositions.Num());

	if (mPointPosData.Num() < pointPositions.Num()) {
		mPointPosData.Empty();
		mPointPosData.AddUninitialized(mPointCount); // 4 as we have bgra
	}

	for (int i = 0; i < pointColors.Num(); ++i) {

		mPointPosData[i].A = pointPositions[i].Z;	//#ToDo: Improve (bottleneck!?)
		mPointPosData[i].G = pointPositions[i].X;
		mPointPosData[i].B = pointPositions[i].Y;
		mPointPosData[i].R = pointPositions[i].Z;
	}

	mPointPosDataPointer = &mPointPosData;

	if (mColorData.Num() < pointColors.Num()) {
		mColorData.Empty();
		mColorData.AddUninitialized(mPointCount * 4); // 4 as we have bgra
	}

	for (int i = 0; i < pointColors.Num(); ++i) {

		mColorData[i * 4] = pointColors[i].R;
		mColorData[i * 4 + 1] = pointColors[i].G;
		mColorData[i * 4 + 2] = pointColors[i].B;
		mColorData[i * 4 + 3] = pointColors[i].A;
	}

	mColorDataPointer = &mColorData;

	if (sortDataEveryFrame)
		SortPointCloudData();
	UpdateStreamingTextures();
}

void FPointCloudStreamingCore::Update(float deltaTime) {

	UpdateShaderParameter();

	TotalElapsedTime += deltaTime;
}

void FPointCloudStreamingCore::SortPointCloudData() {

	SCOPE_CYCLE_COUNTER(STAT_SortPointCloudData);

	// # ToDo
}

void FPointCloudStreamingCore::UpdateStreamingTextures()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateTextureRegions);

	if (mColorData.Num() == 0 || mPointPosDataPointer->Num() == 0 || !mPointPosTexture || !mColorTexture || !mPointScalingTexture)
		return;

	mPointPosTexture->UpdateTextureRegions(0, 1, mUpdateTextureRegion, mPointPosTexture->GetSizeX() * sizeof(FLinearColor), sizeof(FLinearColor), (uint8*)mPointPosDataPointer->GetData());
	mColorTexture->UpdateTextureRegions(0, 1, mUpdateTextureRegion, mColorTexture->GetSizeX() * sizeof(uint8) * 4, 4, mColorDataPointer->GetData());
	//if(mHasSurfaceReconstructed)
		mPointScalingTexture->UpdateTextureRegions(0, 1, mUpdateTextureRegion, mPointPosTexture->GetSizeX() * sizeof(FVector), sizeof(FVector), (uint8*)mPointScalingData.GetData());
}

void FPointCloudStreamingCore::UpdateShaderParameter()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateShaderTextures);

	if (mColorData.Num() == 0 || mPointPosDataPointer->Num() == 0 || !mPointPosTexture || !mColorTexture || !mPointScalingTexture)
		return;

	if (!mDynamicMatInstance)
		return;

	mDynamicMatInstance->SetTextureParameterValue("PositionTexture", mPointPosTexture);
	mDynamicMatInstance->SetTextureParameterValue("ColorTexture", mColorTexture);
	//if (mHasSurfaceReconstructed)
		mDynamicMatInstance->SetTextureParameterValue("ScalingTexture", mPointScalingTexture);
	mDynamicMatInstance->SetVectorParameterValue("CloudSizeV2", FLinearColor(mPointPosTexture->GetSizeX(), mPointPosTexture->GetSizeY(), 0, 0));
	//mDynamicMatInstance->SetVectorParameterValue("minExtent", _extent.Min);
	//mDynamicMatInstance->SetVectorParameterValue("maxExtent", _extent.Max);
}

FPointCloudStreamingCore::~FPointCloudStreamingCore() {
	//if (mUpdateTextureRegion)
	//	delete mUpdateTextureRegion;
	//if (mPointPosTexture)
	//	delete mPointPosTexture;
	//if (mPointScalingTexture)
	//	delete mPointScalingTexture;
	//if (mColorTexture)
	//	delete mColorTexture;
}