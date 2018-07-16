/*************************************************************************************************
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, 2018
**************************************************************************************************/

#include "CoreMinimal.h"
#include "PointCloudStreamingCore.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "App.h"
#include "Runtime/Engine/Classes/Materials/MaterialInstanceDynamic.h"
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

void FPointCloudStreamingCore::AddSnapshot(TArray<FLinearColor> &pointPositions, TArray<uint8> &pointColors, FVector offsetTranslation, FRotator offsetRotation) {

	check(pointPositions.Num() * 4 == pointColors.Num());

	if (mGlobalStreamCounter + pointPositions.Num() >= PCR_MAXTEXRES * PCR_MAXTEXRES)
		return;
	if (mDeltaTime < mStreamCaptureSteps)
		return;

	Initialize(PCR_MAXTEXRES * PCR_MAXTEXRES);
	InitPointPosBuffer();
	InitColorBuffer();

	FVector transformedPos;

	for (int i = 0; i < pointPositions.Num(); ++i) {

		// Transform point
		transformedPos = FVector(pointPositions[i].G, pointPositions[i].B, pointPositions[i].R);
		transformedPos = offsetRotation.RotateVector(transformedPos);
		transformedPos += offsetTranslation;

		// Add current data to buffer
		if (mPointPosData.IsValidIndex(i)) {
			mPointPosData[mGlobalStreamCounter + i].A = transformedPos.Z;
			mPointPosData[mGlobalStreamCounter + i].G = transformedPos.X;
			mPointPosData[mGlobalStreamCounter + i].B = transformedPos.Y;
			mPointPosData[mGlobalStreamCounter + i].R = transformedPos.Z;
		}
		if (mColorData.IsValidIndex(mGlobalStreamCounter + i)) {
			mColorData[(mGlobalStreamCounter + i) * 4] = pointColors[i * 4];
			mColorData[(mGlobalStreamCounter + i) * 4 + 1] = pointColors[i * 4 + 1];
			mColorData[(mGlobalStreamCounter + i) * 4 + 2] = pointColors[i * 4 + 2];
			mColorData[(mGlobalStreamCounter + i) * 4 + 3] = pointColors[i * 4 + 3];
		}
	}

	mGlobalStreamCounter += pointPositions.Num();
	mPointCount = mGlobalStreamCounter;

	UpdateTextureBuffer();
	mDeltaTime = 0.f;
}

void FPointCloudStreamingCore::SavePointPosDataToTexture(UTextureRenderTarget2D* pointPosRT) {

	if (pointPosRT && mPointPosTexture)
		PixelShading->ExecutePixelShader(pointPosRT, mPointPosTexture->Resource->TextureRHI->GetTexture2D(), FColor::Black, 1.0f);
}

void FPointCloudStreamingCore::SaveColorDataToTexture(UTextureRenderTarget2D* colorsRT) {

	if (colorsRT && mColorTexture)
		PixelShading->ExecutePixelShader(colorsRT, mColorTexture->Resource->TextureRHI->GetTexture2D(), FColor::Black, 1.0f);
}

void FPointCloudStreamingCore::SetInput(TArray<FLinearColor> &pointPositions, TArray<uint8> &pointColors) {

	check(pointPositions.Num() * 4 == pointColors.Num());

	if (pointColors.Num() < pointPositions.Num() * 4)
		pointColors.Reserve(pointPositions.Num() * 4);

	Initialize(pointPositions.Num());

	mPointPosDataPointer = &pointPositions;
	mColorDataPointer = &pointColors;

	UpdateTextureBuffer();
}

void FPointCloudStreamingCore::SetInput(TArray<FLinearColor> &pointPositions, TArray<FColor> &pointColors) {

	ensure(pointPositions.Num() == pointColors.Num());

	Initialize(pointPositions.Num());
	InitColorBuffer();

	for (int i = 0; i < pointColors.Num(); ++i) {

		mColorData[i * 4] = pointColors[i].R;
		mColorData[i * 4 + 1] = pointColors[i].G;
		mColorData[i * 4 + 2] = pointColors[i].B;
		mColorData[i * 4 + 3] = pointColors[i].A;
	}
	mPointPosDataPointer = &pointPositions;

	UpdateTextureBuffer();
}

void FPointCloudStreamingCore::SetInput(TArray<FVector> &pointPositions, TArray<FColor> &pointColors) {

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

		mColorData[i * 4] = pointColors[i].R;
		mColorData[i * 4 + 1] = pointColors[i].G;
		mColorData[i * 4 + 2] = pointColors[i].B;
		mColorData[i * 4 + 3] = pointColors[i].A;
	}

	UpdateTextureBuffer();
}

void FPointCloudStreamingCore::InitColorBuffer()
{
	if (mColorData.Num() != mPointCount * 4) {
		mColorData.Empty();
		mColorData.AddUninitialized(mPointCount * 4); // 4 as we have bgra
		mColorDataPointer = &mColorData;
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

bool FPointCloudStreamingCore::SortPointCloudData() {

	SCOPE_CYCLE_COUNTER(STAT_SortPointCloudData);

	if (!mPointPosTexture || !RenderTarget)
		return false;
	if (mPointCount > PCR_MAX_SORT_COUNT)
		return false;

	mPointPosTexture->WaitForStreaming();

	// Execute shader
	if (PixelShading) {
		FTexture2DRHIRef InputTexture = NULL;

		if (ComputeShading)
		{
			// Send unsorted point position data to compute shader
			ComputeShading->SetPointPosDataReference(mPointPosDataPointer);

			// Execute Sorting Compute Shader
			ComputeShading->ExecuteComputeShader(FVector4(currentCamPos));

			// Get the output texture from the compute shader that we will pass to the pixel shader later
			InputTexture = ComputeShading->GetTexture();
		}

		PixelShading->ExecutePixelShader(RenderTarget, InputTexture, FColor::Red, 1.0f);
		CastedRenderTarget = Cast<UTexture>(RenderTarget);
	}

	return mWasSorted = true;
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
	mWasSorted = false;
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
}

void FPointCloudStreamingCore::UpdateShaderParameter()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateShaderTextures);

	if (!mPointPosTexture || !mColorTexture || !mPointScalingTexture)
		return;
	if (!mDynamicMatInstance)
		return;
	if(CastedRenderTarget)
		CastedRenderTarget->WaitForStreaming();

	if (CastedRenderTarget && mWasSorted)
		mDynamicMatInstance->SetTextureParameterValue("PositionTexture", CastedRenderTarget);
	else
		mDynamicMatInstance->SetTextureParameterValue("PositionTexture", mPointPosTexture);
	mDynamicMatInstance->SetTextureParameterValue("ColorTexture", mColorTexture);
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
		//RenderTarget = nullptr;
		//delete RenderTarget;
	}
	if (CastedRenderTarget) {
		CastedRenderTarget->ReleaseResource();
		//delete CastedRenderTarget;
	}
}