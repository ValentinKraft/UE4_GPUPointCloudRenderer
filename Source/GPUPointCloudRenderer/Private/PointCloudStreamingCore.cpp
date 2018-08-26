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
		mPixelShader->ExecutePixelShader(pointPosRT, mPointPosTexture->Resource->TextureRHI->GetTexture2D(), FColor::Black, 1.0f);
}

void FPointCloudStreamingCore::SaveColorDataToTexture(UTextureRenderTarget2D* colorsRT) {

	if (colorsRT && mColorTexture)
		mPixelShader->ExecutePixelShader(colorsRT, mColorTexture->Resource->TextureRHI->GetTexture2D(), FColor::Black, 1.0f);
}

bool FPointCloudStreamingCore::SetInput(TArray<FLinearColor> &pointPositions, TArray<uint8> &pointColors) {

	check(pointPositions.Num() * 4 == pointColors.Num());

	if (pointColors.Num() < pointPositions.Num() * 4)
		pointColors.Reserve(pointPositions.Num() * 4);

	Initialize(pointPositions.Num());

	mPointPosDataPointer = &pointPositions;
	mColorDataPointer = &pointColors;

	return UpdateTextureBuffer();
}

bool FPointCloudStreamingCore::SetInput(TArray<FLinearColor> &pointPositions, TArray<FColor> &pointColors) {

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

		mColorData[i * 4] = pointColors[i].R;
		mColorData[i * 4 + 1] = pointColors[i].G;
		mColorData[i * 4 + 2] = pointColors[i].B;
		mColorData[i * 4 + 3] = pointColors[i].A;
	}

	return UpdateTextureBuffer();
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

	if (!mPointPosTexture || !mComputeShaderRT)
		return false;
	if (mPointCount > PCR_MAX_SORT_COUNT)
		return false;

	mPointPosTexture->WaitForStreaming();

	// Execute shader
	if (mPixelShader) {
		FTexture2DRHIRef InputTexture = NULL;

		if (mComputeShader)
		{
			// Send unsorted point position data to compute shader
			mComputeShader->SetPointPosDataReference(mPointPosDataPointer);
			// Execute Sorting Compute Shader
			mComputeShader->ExecuteComputeShader(FVector4(currentCamPos));
			// Get the output texture from the compute shader that we will pass to the pixel shader later
			InputTexture = mComputeShader->GetTexture();
		}

		mPixelShader->ExecutePixelShader(mComputeShaderRT, InputTexture, FColor::Red, 1.0f);
		mCastedRT = Cast<UTexture>(mComputeShaderRT);
	}

	return mWasSorted = true;
}

void FPointCloudStreamingCore::Initialize(unsigned int pointCount)
{
	if (pointCount == 0)
		return;

	int32 pointsPerAxis = FMath::CeilToInt(FMath::Sqrt(pointCount));
	// Ensure even sized textures to avoid inaccuracies
	if (pointsPerAxis % 2 == 1) pointsPerAxis++;
	mPointCount = pointsPerAxis * pointsPerAxis;

	// Check if update is neccessary
	if (mPointPosTexture && mColorTexture && mPointScalingTexture && mUpdateTextureRegion)
		if (mPointPosTexture->GetSizeX() == pointsPerAxis && mColorTexture->GetSizeX() == pointsPerAxis && mPointScalingTexture->GetSizeX() == pointsPerAxis)
			return;

	// create point cloud positions texture
	mPointPosTexture = UTexture2D::CreateTransient(pointsPerAxis, pointsPerAxis, EPixelFormat::PF_A32B32G32R32F);
	mPointPosTexture->CompressionSettings = TextureCompressionSettings::TC_HDR;
	mPointPosTexture->SRGB = 0;
	mPointPosTexture->AddToRoot();
	mPointPosTexture->UpdateResource();
#if WITH_EDITOR 
	mPointPosTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
#endif

	// create point cloud scalings texture
	mPointScalingTexture = UTexture2D::CreateTransient(pointsPerAxis, pointsPerAxis, EPixelFormat::PF_A32B32G32R32F);
	mPointScalingTexture->CompressionSettings = TextureCompressionSettings::TC_HDR;
	mPointScalingTexture->SRGB = 0;
	mPointScalingTexture->AddToRoot();
	mPointScalingTexture->UpdateResource();
#if WITH_EDITOR 
	mPointScalingTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
#endif

	// create color texture
	mColorTexture = UTexture2D::CreateTransient(pointsPerAxis, pointsPerAxis, EPixelFormat::PF_B8G8R8A8);
	mColorTexture->CompressionSettings = TextureCompressionSettings::TC_Default;
	mColorTexture->SRGB = 1;
	mColorTexture->AddToRoot();
	mColorTexture->UpdateResource();
#if WITH_EDITOR 
	mColorTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
#endif

	// Create shader
	if (!mComputeShader && currentWorld)
		mComputeShader = new FComputeShader(1.0f, GetUpperPowerOfTwo(pointsPerAxis), GetUpperPowerOfTwo(pointsPerAxis), currentWorld->Scene->GetFeatureLevel());

	if (!mPixelShader && currentWorld)
		mPixelShader = new FPixelShader(FColor::Green, currentWorld->Scene->GetFeatureLevel());

	// Create shader render target
	if (!mComputeShaderRT) {
		mComputeShaderRT = NewObject<UTextureRenderTarget2D>();
		check(mComputeShaderRT);
		mComputeShaderRT->AddToRoot();
		mComputeShaderRT->ClearColor = FLinearColor(1.0f, 0.0f, 1.0f);
		mComputeShaderRT->ClearColor.A = 1.0f;
		mComputeShaderRT->SRGB = 0;
		mComputeShaderRT->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
		mComputeShaderRT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA32f;
		mComputeShaderRT->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
		mComputeShaderRT->InitAutoFormat(GetUpperPowerOfTwo(pointsPerAxis), GetUpperPowerOfTwo(pointsPerAxis));
		mComputeShaderRT->UpdateResourceImmediate(true);
		//mComputeShaderRT->InitCustomFormat(pointsPerAxis, pointsPerAxis, EPixelFormat::PF_A32B32G32R32F, false);
		checkf(mComputeShaderRT != nullptr, TEXT("Unable to create or find valid render target"));
	}

	mPointPosData.Empty();
	mPointPosData.AddUninitialized(mPointCount);
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

bool FPointCloudStreamingCore::UpdateTextureBuffer()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateTextureRegions);

	if (!mColorDataPointer || !mPointPosDataPointer || !mPointPosTexture || !mColorTexture || !mPointScalingTexture)
		return false;
	if (&mPointPosDataPointer == nullptr || &mColorDataPointer == nullptr)
		return false;
	if (mColorDataPointer->Num() == 0 || mPointPosDataPointer->Num() == 0)
		return false;
	if (mColorDataPointer->Num() > mColorTexture->GetSizeX() * mColorTexture->GetSizeY() * 4 || mPointPosDataPointer->Num() > mPointPosTexture->GetSizeX()*mPointPosTexture->GetSizeY())
		return false;

	mPointPosTexture->WaitForStreaming();
	mColorTexture->WaitForStreaming();
	//mPointScalingTexture->WaitForStreaming();

	mPointPosTexture->UpdateTextureRegions(0, 1, mUpdateTextureRegion, mPointPosTexture->GetSizeX() * sizeof(FLinearColor), sizeof(FLinearColor), (uint8*)mPointPosDataPointer->GetData());
	mColorTexture->UpdateTextureRegions(0, 1, mUpdateTextureRegion, mColorTexture->GetSizeX() * sizeof(uint8) * 4, 4, mColorDataPointer->GetData());
	//if(mHasSurfaceReconstructed)
	//	mPointScalingTexture->UpdateTextureRegions(0, 1, mUpdateTextureRegion, mPointPosTexture->GetSizeX() * sizeof(FVector), sizeof(FVector), (uint8*)mPointScalingData.GetData());

	mPointPosTexture->WaitForStreaming();
	mColorTexture->WaitForStreaming();

	return true;
}

void FPointCloudStreamingCore::UpdateShaderParameter()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateShaderTextures);

	if (!mPointPosTexture || !mColorTexture || !mPointScalingTexture)
		return;
	if (!mDynamicMatInstance)
		return;
	if(mCastedRT)
		mCastedRT->WaitForStreaming();

	if (mCastedRT && mWasSorted) {
		mDynamicMatInstance->SetTextureParameterValue("PositionTexture", mCastedRT);
		mDynamicMatInstance->SetScalarParameterValue("TextureSize", mComputeShaderRT->SizeX);
	}
	else if(!mWasSorted){
		mDynamicMatInstance->SetTextureParameterValue("PositionTexture", mPointPosTexture);
		mDynamicMatInstance->SetScalarParameterValue("TextureSize", mPointPosTexture->GetSizeX());
	}
	mDynamicMatInstance->SetTextureParameterValue("ColorTexture", mColorTexture);
	mDynamicMatInstance->SetScalarParameterValue("ColorTextureSize", mColorTexture->GetSizeX());
	//if (mHasSurfaceReconstructed)
	//	mDynamicMatInstance->SetTextureParameterValue("ScalingTexture", mPointScalingTexture);
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

	if (mPointPosTexture)
		delete mPointPosTexture;
	if (mPointScalingTexture)
		delete mPointScalingTexture;
	if (mColorTexture)
		delete mColorTexture;

	if (mComputeShader)
		delete mComputeShader;
	if (mPixelShader)
		delete mPixelShader;
	if (mComputeShaderRT) {
		mComputeShaderRT->ReleaseResource();
		mComputeShaderRT = nullptr;
	}
	if (mCastedRT) {
		mCastedRT->ReleaseResource();
		mCastedRT = nullptr;
	}
}