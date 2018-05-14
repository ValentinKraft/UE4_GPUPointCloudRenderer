/*************************************************************************************************
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, 2018
**************************************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Components/MeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "PointCloudComponent.generated.h"

class FPrimitiveSceneProxy;

/** Struct containing information about a point along the PointCloud */
struct FPointCloudParticle
{
	FPointCloudParticle(): 
        Position(0,0,0)
	{}

	/** Current position of point */
	FVector Position;
};


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GPUPOINTCLOUDRENDEREREDITOR_API UPointCloudComponent : public UInstancedStaticMeshComponent
{
    GENERATED_UCLASS_BODY()

public:
    void setNumPoints(int numPoints);

	//~ Begin UActorComponent Interface.
	virtual void OnRegister() override;
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void SendRenderDynamicData_Concurrent() override;
	virtual void CreateRenderState_Concurrent() override;
    virtual void InitializeComponent() override;
	//~ Begin UActorComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ Begin USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.

	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const override;


	/** How many points the PointCloud has */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Points")
	int32 NumPoints = 0;

	/** How wide the PointCloud geometry is */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Points", meta=(ClampMin = "0.01", UIMin = "0.01", UIMax = "50.0"))
	float PointCloudWidth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Points", meta=(UIMin = "0.1", UIMax = "8"))
    float triangleSize;
    
private:
	
    /** Array of PointCloud particles */
    TArray<FPointCloudParticle>	Points;

	friend class FPointCloudSceneProxy;
};
