/*************************************************************************************************
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, 2018
**************************************************************************************************/

#include "CoreMinimal.h"
#include "PointCloudStreamingCore.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "App.h"
#include "Runtime/Engine/Classes/Materials/MaterialInstanceDynamic.h"
#include "ComputeShaderUsageExample.h"
#include "PixelShaderUsageExample.h"

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

void FPointCloudStreamingCore::AddInputToExistingData(TArray<FLinearColor> &pointPositions, TArray<uint8> &pointColors, FLinearColor offset) {

	check(pointPositions.Num() * 4 == pointColors.Num());

	if (mGlobalStreamCounter + pointPositions.Num() >= MAXTEXRES * MAXTEXRES)
		return;
	if (mDeltaTime < mStreamCaptureSteps)
		return;
	//if (pointColors.Num() < pointPositions.Num() * 4)
	//	pointColors.Reserve(pointPositions.Num() * 4);

	Initialize(MAXTEXRES * MAXTEXRES);

	// Init data if neccessary
	if (mPointPosData.Num() != MAXTEXRES * MAXTEXRES) {
		mPointPosData.Empty();
		mPointPosData.AddUninitialized(MAXTEXRES * MAXTEXRES);
		mPointPosDataPointer = &mPointPosData;
	}

	if (mColorData.Num() != MAXTEXRES * MAXTEXRES * 4) {
		mColorData.Empty();
		mColorData.AddUninitialized(MAXTEXRES * MAXTEXRES * 4); // 4 as we have bgra
		mColorDataPointer = &mColorData;
	}

	// Add offset (e.g. tracking position - camera offset)
	if (offset != FLinearColor::Black)
		for (int i = 0; i < pointPositions.Num(); ++i)
			pointPositions[i] += offset;

	for (int i = 0; i < pointPositions.Num(); ++i) {
		if (mPointPosData.IsValidIndex(i))
			mPointPosData[mGlobalStreamCounter + i] = pointPositions[i];
		if (mColorData.IsValidIndex(mGlobalStreamCounter + i)) {
			mColorData[(mGlobalStreamCounter + i) * 4] = pointColors[i * 4];
			mColorData[(mGlobalStreamCounter + i) * 4 + 1] = pointColors[i * 4 + 1];
			mColorData[(mGlobalStreamCounter + i) * 4 + 2] = pointColors[i * 4 + 2];
			mColorData[(mGlobalStreamCounter + i) * 4 + 3] = pointColors[i * 4 + 3];
		}
	}

	mGlobalStreamCounter += pointPositions.Num();
	mPointCount = mGlobalStreamCounter;

	//UE_LOG(PointCloudRenderer, Log, TEXT("Collecting point cloud from Kinect. Current point count: %d"), mGlobalStreamCounter);

	UpdateTextureBuffer();
	mDeltaTime = 0.f;
}

void FPointCloudStreamingCore::SetInput(TArray<FLinearColor> &pointPositions, TArray<uint8> &pointColors, bool sortData) {

	check(pointPositions.Num() * 4 == pointColors.Num());

	if (pointColors.Num() < pointPositions.Num() * 4)
		pointColors.Reserve(pointPositions.Num() * 4);

	Initialize(pointPositions.Num());

	mPointPosDataPointer = &pointPositions;
	mColorDataPointer = &pointColors;

	if (sortData)
		SortPointCloudData();
	UpdateTextureBuffer();
}

void FPointCloudStreamingCore::SetInput(TArray<FLinearColor> &pointPositions, TArray<FColor> &pointColors, bool sortData) {

	ensure(pointPositions.Num() == pointColors.Num());

	Initialize(pointPositions.Num());

	if (mColorData.Num() != mPointCount * 4) {
		mColorData.Empty();
		mColorData.AddUninitialized(mPointCount * 4); // 4 as we have bgra
		mColorDataPointer = &mColorData;
	}

	for (int i = 0; i < pointColors.Num(); ++i) {

		mColorData[i * 4] = pointColors[i].R;
		mColorData[i * 4 + 1] = pointColors[i].G;
		mColorData[i * 4 + 2] = pointColors[i].B;
		mColorData[i * 4 + 3] = pointColors[i].A;
	}
	mPointPosDataPointer = &pointPositions;

	if (sortData)
		SortPointCloudData();
	UpdateTextureBuffer();
}

void FPointCloudStreamingCore::SetInput(TArray<FVector> &pointPositions, TArray<FColor> &pointColors, bool sortData) {

	ensure(pointPositions.Num() == pointColors.Num());

	Initialize(pointPositions.Num());

	if (mPointPosData.Num() != mPointCount) {
		mPointPosData.Empty();
		mPointPosData.AddUninitialized(mPointCount);
		mPointPosDataPointer = &mPointPosData;
	}

	if (mColorData.Num() != mPointCount * 4) {
		mColorData.Empty();
		mColorData.AddUninitialized(mPointCount * 4); // 4 as we have bgra
		mColorDataPointer = &mColorData;
	}

	for (int i = 0; i < pointPositions.Num(); ++i) {

		mPointPosData[i].A = pointPositions[i].Z;	//#ToDo: Improve (bottleneck!?)
		mPointPosData[i].G = pointPositions[i].X;
		mPointPosData[i].B = pointPositions[i].Y;
		mPointPosData[i].R = pointPositions[i].Z;
	}

	for (int i = 0; i < pointColors.Num(); ++i) {

		mColorData[i * 4] = pointColors[i].R;
		mColorData[i * 4 + 1] = pointColors[i].G;
		mColorData[i * 4 + 2] = pointColors[i].B;
		mColorData[i * 4 + 3] = pointColors[i].A;
	}

	if (sortData)
		SortPointCloudData();
	UpdateTextureBuffer();
}

void FPointCloudStreamingCore::SortPointCloudData() {

	SCOPE_CYCLE_COUNTER(STAT_SortPointCloudData);

	if (!mPointPosTexture || !RenderTarget)
		return;

	mPointPosTexture->WaitForStreaming();

	// Execute shader
	if (PixelShading) {
		FTexture2DRHIRef InputTexture = NULL;

		if (ComputeShading)
		{
			// Send unsorted point position texture to compute shader
			ComputeShading->SetPointPosTextureReference(mPointPosTexture->Resource->TextureRHI->GetTexture2D());

			ComputeShading->ExecuteComputeShader(TotalElapsedTime);

			// Get the output texture from the compute shader that we will pass to the pixel shader later
			InputTexture = ComputeShading->GetTexture();
		}

		PixelShading->ExecutePixelShader(RenderTarget, InputTexture, FColor::Red, 1.0f);
		CastedRenderTarget = Cast<UTexture>(RenderTarget);
	}
}

void FPointCloudStreamingCore::Initialize(unsigned int pointCount)
{
	if (pointCount == 0)
		return;

	int32 pointsPerAxis = FMath::CeilToInt(FMath::Sqrt(pointCount));
	mPointCount = pointsPerAxis * pointsPerAxis;

	// Check if update is neccessary
	if (mPointPosTexture && mColorTexture && mPointScalingTexture && mUpdateTextureRegion && ComputeShading && PixelShading)
		if (mPointPosTexture->GetSizeX() == pointsPerAxis && mColorTexture->GetSizeX() == pointsPerAxis && mPointScalingTexture->GetSizeX() == pointsPerAxis)
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

	// Create shader
	if (!ComputeShading && currentWorld)
		ComputeShading = new FComputeShaderUsageExample(1.0f, GetUpperPowerOfTwo(pointsPerAxis), GetUpperPowerOfTwo(pointsPerAxis), currentWorld->Scene->GetFeatureLevel());

	if (!PixelShading && currentWorld)
		PixelShading = new FPixelShaderUsageExample(FColor::Green, currentWorld->Scene->GetFeatureLevel());

	// Create shader render target
	if (!RenderTarget) {
		RenderTarget = NewObject<UTextureRenderTarget2D>();
		check(RenderTarget);
		RenderTarget->AddToRoot();
		RenderTarget->ClearColor = FLinearColor(1.0f, 0.0f, 1.0f);
		RenderTarget->ClearColor.A = 1.0f;
		RenderTarget->SRGB = 0;
		RenderTarget->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
		RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA32f;
		RenderTarget->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
		RenderTarget->InitAutoFormat(GetUpperPowerOfTwo(pointsPerAxis), GetUpperPowerOfTwo(pointsPerAxis));
		RenderTarget->UpdateResourceImmediate(true);
		//RenderTarget->InitCustomFormat(pointsPerAxis, pointsPerAxis, EPixelFormat::PF_A32B32G32R32F, false);
		checkf(RenderTarget != nullptr, TEXT("Unable to create or find valid render target"));
	}

	mPointPosData.Empty();
	mPointPosData.AddUninitialized(mPointCount);
	mPointPosDataPointer = &mPointPosData;
	if (!mPointPosDataPointer)
		mPointPosDataPointer = &mPointPosData;
	if (!mColorDataPointer)
		mColorDataPointer = &mColorData;

	mPointPosTexture->WaitForStreaming();
	mColorTexture->WaitForStreaming();
	mPointScalingTexture->WaitForStreaming();

	mPointScalingData.Empty();
	mPointScalingData.Init(FVector::OneVector, mPointCount);

	if (mUpdateTextureRegion) delete mUpdateTextureRegion; mUpdateTextureRegion = nullptr;
	mUpdateTextureRegion = new FUpdateTextureRegion2D(0, 0, 0, 0, pointsPerAxis, pointsPerAxis);

	mGlobalStreamCounter = 0;
}

void FPointCloudStreamingCore::UpdateTextureBuffer()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateTextureRegions);

	if (!mColorDataPointer || !mPointPosDataPointer || !mPointPosTexture || !mColorTexture || !mPointScalingTexture)
		return;
	if (mColorDataPointer->Num() == 0 || mPointPosDataPointer->Num() == 0)
		return;
	if (mColorDataPointer->Num() > mColorTexture->GetSizeX() * mColorTexture->GetSizeY() * 4 || mPointPosDataPointer->Num() > mPointPosTexture->GetSizeX()*mPointPosTexture->GetSizeY())
		return;

	mPointPosTexture->WaitForStreaming();
	mColorTexture->WaitForStreaming();
	//mPointScalingTexture->WaitForStreaming();

	mPointPosTexture->UpdateTextureRegions(0, 1, mUpdateTextureRegion, mPointPosTexture->GetSizeX() * sizeof(FLinearColor), sizeof(FLinearColor), (uint8*)mPointPosDataPointer->GetData());
	mColorTexture->UpdateTextureRegions(0, 1, mUpdateTextureRegion, mColorTexture->GetSizeX() * sizeof(uint8) * 4, 4, mColorDataPointer->GetData());
	//if(mHasSurfaceReconstructed)
	//	mPointScalingTexture->UpdateTextureRegions(0, 1, mUpdateTextureRegion, mPointPosTexture->GetSizeX() * sizeof(FVector), sizeof(FVector), (uint8*)mPointScalingData.GetData());

	mPointPosTexture->WaitForStreaming();
	mColorTexture->WaitForStreaming();
}

void FPointCloudStreamingCore::UpdateShaderParameter()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateShaderTextures);

	if (!mPointPosTexture || !mColorTexture || !mPointScalingTexture)
		return;
	if (!mDynamicMatInstance)
		return;

	mDynamicMatInstance->SetTextureParameterValue("PositionTexture", mPointPosTexture);
	mDynamicMatInstance->SetTextureParameterValue("ColorTexture", mColorTexture);
	mDynamicMatInstance->SetTextureParameterValue("CSTexture", CastedRenderTarget);
	//if (mHasSurfaceReconstructed)
	//	mDynamicMatInstance->SetTextureParameterValue("ScalingTexture", mPointScalingTexture);
	mDynamicMatInstance->SetVectorParameterValue("CloudSizeV2", FLinearColor(mPointPosTexture->GetSizeX(), mPointPosTexture->GetSizeY(), 0, 0));
	mDynamicMatInstance->SetVectorParameterValue("minExtent", mExtent.Min);
	mDynamicMatInstance->SetVectorParameterValue("maxExtent", mExtent.Max);
}

unsigned int FPointCloudStreamingCore::GetUpperPowerOfTwo(unsigned int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

void FPointCloudStreamingCore::FreeData()
{
	mGlobalStreamCounter = 0;
	mPointPosData.Empty();
	mPointPosDataPointer = nullptr;
	mColorData.Empty();
	mColorDataPointer = nullptr;
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

	if (ComputeShading)
		delete ComputeShading;
	if (PixelShading)
		delete PixelShading;
	if (RenderTarget) {
		RenderTarget->ReleaseResource();
		//delete RenderTarget;
	}
	if (CastedRenderTarget) {
		CastedRenderTarget->ReleaseResource();
		//delete CastedRenderTarget;
	}
}