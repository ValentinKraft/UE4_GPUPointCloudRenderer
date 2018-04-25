/*************************************************************************************************
* Copyright (C) Valentin Kraft - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, February 2018
**************************************************************************************************/

#include "CoreMinimal.h"
#include "IPointCloudRenderer.h"
#include "PointCloudStreamingCore.h"
#include "Runtime/Engine/Classes/Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "App.h"


using namespace std;
#undef UpdateResource


//////////////////////
// MAIN FUNCTIONS ////
//////////////////////


void FPointCloudStreamingCore::FillPointCloudWithRandomPoints(int32 pointsPerAxis, float extent) {

	//IPointCloudCore::FillPointCloudWithRandomPoints(pointsPerAxis, extent);
	InitializeStreaming(pointsPerAxis*pointsPerAxis);
	UpdateStreamingBuffers();
	SortPointCloudData();
	UpdateStreamingTextures();
}


//////////////////////
// STREAMING /////////
//////////////////////


void FPointCloudStreamingCore::InitializeStreaming(unsigned int pointCount)
{
	int32 pointsPerAxis = FMath::CeilToInt(FMath::Sqrt(pointCount));
	auto newPointCount = pointsPerAxis * pointsPerAxis;

	// Check if update is neccessary
	if (mColorData.Num() == newPointCount * 4 && mPointPosDataPointer->Num() == newPointCount && mPointPosTexture && mColorTexture && mPointScalingTexture)
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

	// cpu buffer
	mPointPosData.Init(FLinearColor::Black, newPointCount);
	mPointPosDataPointer = &mPointPosData;
	mColorData.AddUninitialized(newPointCount * 4); // 4 as we have bgra
	mPointScalingData.Init(FVector::OneVector, newPointCount);

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

void FPointCloudStreamingCore::SetCustomPointInput(TArray<FLinearColor> &pointPositions, TArray<FColor> &pointColors) {
	
	InitializeStreaming(pointPositions.Num());

	ensure(pointPositions.Num() == pointColors.Num());
	ensure(mPointPosDataPointer->Num() >= pointPositions.Num() && mColorData.Num() >= pointColors.Num());

	mPointPosDataPointer = &pointPositions;

	for (int i = 0; i < pointColors.Num(); ++i) {		

		mColorData[i * 4] = pointColors[i].R;
		mColorData[i * 4 + 1] = pointColors[i].G;
		mColorData[i * 4 + 2] = pointColors[i].B;
		mColorData[i * 4 + 3] = pointColors[i].A;
	}

}

void FPointCloudStreamingCore::Update(bool isDynamicPointCloud, bool sortDataEveryFrame, float deltaTime) {

	if(sortDataEveryFrame)
		SortPointCloudData();
	if (isDynamicPointCloud)
		UpdateStreamingTextures();
	UpdateShaderParameter();

	TotalElapsedTime += deltaTime;
}

void FPointCloudStreamingCore::UpdateStreamingBuffers()
{
	//SCOPE_CYCLE_COUNTER(STAT_FillCPUBuffer);

	//if (mColorData.Num() == 0 || mPointPosDataPointer->Num() == 0 || !mPointPosTexture || !mColorTexture || cloud->points.size() > (*mPointPosDataPointer).Num())		//#ToDo: exceptions o.s.
	//	return;

	//for (int i = 0; i < cloud->points.size(); ++i) {
	//	(*mPointPosDataPointer)[i].A = cloud->points[i].z;	//#ToDo: Improve (bottleneck!?)
	//	(*mPointPosDataPointer)[i].G = cloud->points[i].x;
	//	(*mPointPosDataPointer)[i].B = cloud->points[i].y;
	//	(*mPointPosDataPointer)[i].R = cloud->points[i].z;

	//	mColorData[i * 4] = cloud->points[i].r;
	//	mColorData[i * 4 + 1] = cloud->points[i].g;
	//	mColorData[i * 4 + 2] = cloud->points[i].b;
	//	mColorData[i * 4 + 3] = cloud->points[i].a;
	//}
	
}

void FPointCloudStreamingCore::SortPointCloudData() {

	//SCOPE_CYCLE_COUNTER(STAT_SortPointCloudData);

}

void FPointCloudStreamingCore::UpdateStreamingTextures()
{
	//SCOPE_CYCLE_COUNTER(STAT_UpdateTextureRegions);

	if (mColorData.Num() == 0 || mPointPosDataPointer->Num() == 0 || !mPointPosTexture || !mColorTexture || !mPointScalingTexture)
		return;

	mPointPosTexture->UpdateTextureRegions(0, 1, mUpdateTextureRegion, mPointPosTexture->GetSizeX() * sizeof(FLinearColor), sizeof(FLinearColor), (uint8*)mPointPosDataPointer->GetData());
	mColorTexture->UpdateTextureRegions(0, 1, mUpdateTextureRegion, mColorTexture->GetSizeX() * sizeof(uint8) * 4, 4, mColorData.GetData());
	//if(mHasSurfaceReconstructed)
		mPointScalingTexture->UpdateTextureRegions(0, 1, mUpdateTextureRegion, mPointPosTexture->GetSizeX() * sizeof(FVector), sizeof(FVector), (uint8*)mPointScalingData.GetData());
}

void FPointCloudStreamingCore::UpdateShaderParameter()
{
	//SCOPE_CYCLE_COUNTER(STAT_UpdateShaderTextures);

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
	if (mUpdateTextureRegion)
		delete mUpdateTextureRegion;
	if (mPointPosTexture)
		delete mPointPosTexture;
	if (mPointScalingTexture)
		delete mPointScalingTexture;
	if (mColorTexture)
		delete mColorTexture;
}