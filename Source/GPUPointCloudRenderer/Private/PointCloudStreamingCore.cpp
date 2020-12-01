/*************************************************************************************************
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, 2018
**************************************************************************************************/

#include "PointCloudStreamingCore.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
//#include "App.h"
#include "Runtime/Engine/Classes/Materials/MaterialInstanceDynamic.h"
//#include "ComputeShaderUsageExample.h"
//#include "PixelShaderUsageExample.h"

using namespace std;
#undef UpdateResource

DECLARE_CYCLE_STAT(TEXT("Update Texture Regions"), STAT_UpdateTextureRegions, STATGROUP_GPUPCR);
DECLARE_CYCLE_STAT(TEXT("Sort Point Cloud Data"), STAT_SortPointCloudData, STATGROUP_GPUPCR);
DECLARE_CYCLE_STAT(TEXT("Update Shader Textures"), STAT_UpdateShaderTextures, STATGROUP_GPUPCR);


//////////////////////
// MAIN FUNCTIONS ////
//////////////////////

void FPointCloudStreamingCore::AddSnapshot(TArray<FLinearColor> &pointPositions, TArray<uint8> &pointColors, FVector offsetTranslation, FRotator offsetRotation) {

	check(pointPositions.Num() * 4 == pointColors.Num());

	if (mGlobalStreamCounter + pointPositions.Num() >= MAXTEXRES * MAXTEXRES)
		return;
	if (mDeltaTime < mStreamCaptureSteps)
		return;

	Initialize(MAXTEXRES * MAXTEXRES);
	InitPointPosBuffer();
	InitColorBuffer();

	FVector tempPos;

	for (int i = 0; i < pointPositions.Num(); ++i) {
		
		// Transform point
		tempPos = FVector(pointPositions[i].G, pointPositions[i].B, pointPositions[i].R);
		tempPos = offsetRotation.RotateVector(tempPos);
		tempPos += offsetTranslation;

		// Add current data to buffer
		if (mPointPosData.IsValidIndex(i)) {
			mPointPosData[mGlobalStreamCounter + i].A = tempPos.Z;
			mPointPosData[mGlobalStreamCounter + i].G = tempPos.X;
			mPointPosData[mGlobalStreamCounter + i].B = tempPos.Y;
			mPointPosData[mGlobalStreamCounter + i].R = tempPos.Z;
		}
		if (mPointColorData.IsValidIndex(mGlobalStreamCounter + i)) {
			mPointColorData[(mGlobalStreamCounter + i) * 4] = pointColors[i * 4];
			mPointColorData[(mGlobalStreamCounter + i) * 4 + 1] = pointColors[i * 4 + 1];
			mPointColorData[(mGlobalStreamCounter + i) * 4 + 2] = pointColors[i * 4 + 2];
			mPointColorData[(mGlobalStreamCounter + i) * 4 + 3] = pointColors[i * 4 + 3];
		}
	}

	mGlobalStreamCounter += pointPositions.Num();
	mPointCount = mGlobalStreamCounter;

	UpdateTextureBuffer();
	mDeltaTime = 0.f;
}

bool FPointCloudStreamingCore::SetInput(TArray<FLinearColor> &pointPositions, TArray<uint8> &pointColors) {

	check(pointPositions.Num() * 4 == pointColors.Num());

	if (pointColors.Num() < pointPositions.Num() * 4)
		pointColors.SetNumZeroed(pointPositions.Num() * 4);

	Initialize(pointPositions.Num());
	mPointPosDataPointer = &pointPositions;
	mPointColorDataPointer = &pointColors;

	// Resize arrays with zero values if neccessary
	if (pointPositions.Num() < (int32)mPointCount)
		pointPositions.SetNumZeroed(mPointCount);
	if (pointColors.Num() < (int32)mPointCount*4)
		pointColors.SetNumZeroed(mPointCount*4);

	return UpdateTextureBuffer();
}

bool FPointCloudStreamingCore::SetInput(TArray<FLinearColor> &pointPositions, TArray<FColor> &pointColors) {

	ensure(pointPositions.Num() == pointColors.Num());

	Initialize(pointPositions.Num());
	InitColorBuffer();

	for (int i = 0; i < pointColors.Num(); ++i) {

		mPointColorData[i * 4] = pointColors[i].R;
		mPointColorData[i * 4 + 1] = pointColors[i].G;
		mPointColorData[i * 4 + 2] = pointColors[i].B;
		mPointColorData[i * 4 + 3] = pointColors[i].A;
	}
	mPointPosDataPointer = &pointPositions;

	// Resize arrays with zero values if neccessary
	if (pointPositions.Num() < (int32)mPointCount)
		pointPositions.SetNumZeroed(mPointCount);

	return UpdateTextureBuffer();
}

bool FPointCloudStreamingCore::SetInput(TArray<FVector> &pointPositions, TArray<FColor> &pointColors) {

	ensure(pointPositions.Num() == pointColors.Num());

	Initialize(pointPositions.Num());
	InitPointPosBuffer();
	InitColorBuffer();

	for (int i = 0; i < pointPositions.Num(); ++i) {

		mPointPosData[i].A = pointPositions[i].Z;	//#ToDo: Improve (bottleneck!?)
		mPointPosData[i].G = pointPositions[i].X;
		mPointPosData[i].B = pointPositions[i].Y;
		mPointPosData[i].R = pointPositions[i].Z;
	}

	for (int i = 0; i < pointColors.Num(); ++i) {

		mPointColorData[i * 4] = pointColors[i].R;
		mPointColorData[i * 4 + 1] = pointColors[i].G;
		mPointColorData[i * 4 + 2] = pointColors[i].B;
		mPointColorData[i * 4 + 3] = pointColors[i].A;
	}
	
	return UpdateTextureBuffer();
}

void FPointCloudStreamingCore::InitColorBuffer()
{
	if (mPointColorData.Num() != mPointCount * 4) {
		mPointColorData.Empty();
		mPointColorData.AddUninitialized(mPointCount * 4); // 4 as we have bgra
		mPointColorDataPointer = &mPointColorData;
	}
}

void FPointCloudStreamingCore::InitPointPosBuffer()
{
	if (mPointPosData.Num() != mPointCount) {
		mPointPosData.Empty();
		mPointPosData.AddUninitialized(mPointCount);
		mPointPosDataPointer = &mPointPosData;
	}
}

void FPointCloudStreamingCore::SortPointCloudData() {

	SCOPE_CYCLE_COUNTER(STAT_SortPointCloudData);

}

void FPointCloudStreamingCore::Initialize(unsigned int pointCount)
{
	if (pointCount == 0)
		return;

	int32 pointsPerAxis = FMath::CeilToInt(FMath::Sqrt(pointCount));
	// Ensure even-sized, power-of-two textures to avoid inaccuracies
	if (pointsPerAxis % 2 == 1) pointsPerAxis++;
	pointsPerAxis = GetUpperPowerOfTwo(pointsPerAxis);

	// Check if update is neccessary
	if (mPointPosTexture && mPointColorTexture && mPointScalingTexture && mUpdateTextureRegion)
		if (mPointPosTexture->GetSizeX() == pointsPerAxis && mPointColorTexture->GetSizeX() == pointsPerAxis && mPointScalingTexture->GetSizeX() == pointsPerAxis)
			return;

	mPointCount = pointsPerAxis * pointsPerAxis;

	ResetPointData(pointsPerAxis);
	CreateTextures(pointsPerAxis);

	mGlobalStreamCounter = 0;
}

void FPointCloudStreamingCore::ResetPointData(const int32 &pointsPerAxis)
{
	mPointPosData.Empty();
	mPointPosData.AddUninitialized(mPointCount);
	mPointPosDataPointer = &mPointPosData;

	mPointColorData.Empty();
	mPointColorData.AddUninitialized(mPointCount * 4);
	mPointColorDataPointer = &mPointColorData;

	mPointScalingData.Empty();
	mPointScalingData.Init(FVector::OneVector, mPointCount);

	if (mUpdateTextureRegion) delete mUpdateTextureRegion; mUpdateTextureRegion = nullptr;
	mUpdateTextureRegion = new FUpdateTextureRegion2D(0, 0, 0, 0, pointsPerAxis, pointsPerAxis);
}

void FPointCloudStreamingCore::CreateTextures(const int32 &pointsPerAxis)
{
	// create point cloud positions texture
	mPointPosTexture = UTexture2D::CreateTransient(pointsPerAxis, pointsPerAxis, EPixelFormat::PF_A32B32G32R32F);
	mPointPosTexture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
	mPointPosTexture->SRGB = 0;
	mPointPosTexture->AddToRoot();
	mPointPosTexture->UpdateResource();
#if WITH_EDITOR 
	mPointPosTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
#endif

	// create point cloud scalings texture
	mPointScalingTexture = UTexture2D::CreateTransient(pointsPerAxis, pointsPerAxis, EPixelFormat::PF_A32B32G32R32F);
	mPointScalingTexture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
	mPointScalingTexture->SRGB = 0;
	mPointScalingTexture->AddToRoot();
	mPointScalingTexture->UpdateResource();
#if WITH_EDITOR 
	mPointScalingTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
#endif

	// create color texture
	mPointColorTexture = UTexture2D::CreateTransient(pointsPerAxis, pointsPerAxis, EPixelFormat::PF_B8G8R8A8);
	mPointColorTexture->CompressionSettings = TextureCompressionSettings::TC_Default;
	mPointColorTexture->SRGB = 1;
	mPointColorTexture->AddToRoot();
	mPointColorTexture->UpdateResource();
#if WITH_EDITOR 
	mPointColorTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
#endif

	mPointPosData.Empty();
	mPointPosData.AddUninitialized(mPointCount);
	if (!mPointPosDataPointer)
		mPointPosDataPointer = &mPointPosData;
	if (!mPointColorDataPointer)
		mPointColorDataPointer = &mPointColorData;

	mPointPosTexture->WaitForStreaming();
	mPointColorTexture->WaitForStreaming();
	mPointScalingTexture->WaitForStreaming();

	mPointScalingData.Empty();
	mPointScalingData.Init(FVector::OneVector, mPointCount);

	if (mUpdateTextureRegion) delete mUpdateTextureRegion; mUpdateTextureRegion = nullptr;
	mUpdateTextureRegion = new FUpdateTextureRegion2D(0, 0, 0, 0, pointsPerAxis, pointsPerAxis);

	mGlobalStreamCounter = 0;
}

bool FPointCloudStreamingCore::UpdateTextureBuffer()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateTextureRegions);

	if (!mPointColorDataPointer || !mPointPosDataPointer || !mPointPosTexture || !mPointColorTexture || !mPointScalingTexture)
		return false;
	if (mPointPosDataPointer == nullptr || mPointColorDataPointer == nullptr)
		return false;
	if (mPointColorDataPointer->Num() == 0 || mPointPosDataPointer->Num() == 0)
		return false;
	if (mPointColorDataPointer->Num() > mPointColorTexture->GetSizeX() * mPointColorTexture->GetSizeY() * 4 || mPointPosDataPointer->Num() > mPointPosTexture->GetSizeX()*mPointPosTexture->GetSizeY())
		return false;
	if (mUpdateTextureRegion == nullptr)
		return false;

	mPointPosTexture->WaitForStreaming();
	mPointColorTexture->WaitForStreaming();
	//mPointScalingTexture->WaitForStreaming();

	mPointPosTexture->UpdateTextureRegions(0, 1, mUpdateTextureRegion, mPointPosTexture->GetSizeX() * sizeof(FLinearColor), sizeof(FLinearColor), (uint8*)mPointPosDataPointer->GetData());
	mPointColorTexture->UpdateTextureRegions(0, 1, mUpdateTextureRegion, mPointColorTexture->GetSizeX() * sizeof(uint8) * 4, 4, mPointColorDataPointer->GetData());
	//if(mHasSurfaceReconstructed)
	//	mPointScalingTexture->UpdateTextureRegions(0, 1, mUpdateTextureRegion, mPointPosTexture->GetSizeX() * sizeof(FVector), sizeof(FVector), (uint8*)mPointScalingData.GetData());

	mPointPosTexture->WaitForStreaming();
	mPointColorTexture->WaitForStreaming();

	return true;
}

void FPointCloudStreamingCore::UpdateShaderParameter()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateShaderTextures);

	if (!mPointPosTexture || !mPointColorTexture || !mPointScalingTexture)
		return;
	if (!mDynamicMatInstance)
		return;

	mDynamicMatInstance->SetTextureParameterValue("PositionTexture", mPointPosTexture);
	mDynamicMatInstance->SetTextureParameterValue("ColorTexture", mPointColorTexture);
	//if (mHasSurfaceReconstructed)
	//	mDynamicMatInstance->SetTextureParameterValue("ScalingTexture", mPointScalingTexture);
	mDynamicMatInstance->SetScalarParameterValue("TextureSize", (float)mPointPosTexture->GetSizeX());
	mDynamicMatInstance->SetVectorParameterValue("minExtent", mExtent.Min);
	mDynamicMatInstance->SetVectorParameterValue("maxExtent", mExtent.Max);
}

void FPointCloudStreamingCore::FreeData()
{
	mGlobalStreamCounter = 0;
	mPointPosData.Empty();
	mPointPosDataPointer = nullptr;
	mPointColorData.Empty();
	mPointColorDataPointer = nullptr;
	mPointScalingData.Empty();
	if (mUpdateTextureRegion) delete mUpdateTextureRegion; mUpdateTextureRegion = nullptr;
}

FPointCloudStreamingCore::~FPointCloudStreamingCore() {

	FreeData();

	//if (mPointPosTexture)
	//	delete mPointPosTexture;
	//if (mPointScalingTexture)
	//	delete mPointScalingTexture;
	//if (mColorTexture)
	//	delete mColorTexture;
}