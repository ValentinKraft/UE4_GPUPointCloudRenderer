/*************************************************************************************************
* Written by Valentin Kraft <valentin.kraft@online.de>, http://www.valentinkraft.de, 2018
**************************************************************************************************/

#include "PointCloudComponent.h"
#include "EngineGlobals.h"
#include "PrimitiveViewRelevance.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "WorldCollision.h"
#include "PrimitiveSceneProxy.h"
#include "VertexFactory.h"
#include "MaterialShared.h"
#include "SceneManagement.h"
#include "Engine/CollisionProfile.h"
#include "Materials/Material.h"
#include "LocalVertexFactory.h"
#include "Engine/Engine.h"
#include "DynamicMeshBuilder.h"

const float sqrt3 = FMath::Sqrt(3);

/** Vertex Buffer */
class FPointCloudVertexBuffer : public FVertexBuffer
{
public:
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		VertexBufferRHI = RHICreateVertexBuffer(NumVerts * sizeof(FDynamicMeshVertex), BUF_Dynamic, CreateInfo);
	}

	int32 NumVerts;
};

/** Index Buffer */
class FPointCloudIndexBuffer : public FIndexBuffer
{
public:
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(int32), NumIndices * sizeof(int32), BUF_Dynamic, CreateInfo);
	}

	int32 NumIndices;
};

/** Vertex Factory */
class FPointCloudVertexFactory : public FLocalVertexFactory
{
public:

	FPointCloudVertexFactory()
	{}


	/** Initialization */
	void Init(const FPointCloudVertexBuffer* VertexBuffer)
	{
		if(IsInRenderingThread())
		{
			// Initialize the vertex factory's stream components.
			FDataType NewData;
			NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
			NewData.TextureCoordinates.Add(
					FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
			);
			NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
			NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
			SetData(NewData);
		}
		else
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
					InitPointCloudVertexFactory,
					FPointCloudVertexFactory*,VertexFactory,this,
			const FPointCloudVertexBuffer*,VertexBuffer,VertexBuffer,
					{
							// Initialize the vertex factory's stream components.
							FDataType NewData;
					NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
					NewData.TextureCoordinates.Add(
					FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
					);
					NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
					NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
					VertexFactory->SetData(NewData);
					});
		}
	}
};

/** Dynamic data sent to render thread */
struct FPointCloudDynamicData
{
	/** Array of points */
	TArray<FVector> PointCloudPoints;
};

//////////////////////////////////////////////////////////////////////////
// FPointCloudSceneProxy

class FPointCloudSceneProxy : public FPrimitiveSceneProxy
{
public:

	FPointCloudSceneProxy(UPointCloudComponent* Component)
			: FPrimitiveSceneProxy(Component)
			, Material(NULL)
			, DynamicData(NULL)
			, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
			, NumPoints(Component->NumPoints)
			, PointCloudWidth(Component->PointCloudWidth)
            , triangleSize(Component->triangleSize)
	{
        mComponent = Component;

		VertexBuffer.NumVerts = GetRequiredVertexCount();
		IndexBuffer.NumIndices = GetRequiredIndexCount();

		// Init vertex factory
		VertexFactory.Init(&VertexBuffer);

		// Enqueue initialization of render resource
		BeginInitResource(&VertexBuffer);
		BeginInitResource(&IndexBuffer);
		BeginInitResource(&VertexFactory);

		// Grab material
		Material = Component->GetMaterial(0);
		if(Material == NULL)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}
	}

	virtual ~FPointCloudSceneProxy()
	{
		VertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();

		if(DynamicData != NULL)
		{
			delete DynamicData;
		}
	}

	int32 GetRequiredVertexCount() const
	{
		return NumPoints * 3;
	}

	int32 GetRequiredIndexCount() const
	{
		return GetRequiredVertexCount();
	}

    inline static TArray<FDynamicMeshVertex> triangle(const FVector& Point, const FColor& Color, const float triangleSize) {
        float x = Point.X;
        float y = Point.Y;
        float z = Point.Z;

        // construct equilateral triangle with x, y, z as center and normal facing z
        float a = triangleSize; // side lenght
        float r = sqrt3 / 6 * a; // radius of inscribed circle
        //float h_minus_r = a / sqrt3; // from center to tip. height - r

        FDynamicMeshVertex v1;
        v1.Position = FVector(x - a / 2.f, y - r, z+0);
        v1.Color = Color;
        v1.TextureCoordinate = FVector2D(- 1 / 2.f, - sqrt3 / 6);
        FDynamicMeshVertex v2;
        v2.Position = FVector(x + a / 2.f, y - r, z+0);
        v2.Color = Color;
        v2.TextureCoordinate = FVector2D(1 / 2.f, -sqrt3 / 6);
        FDynamicMeshVertex v3;
        v3.Position = FVector(x, y + a / sqrt3, z+0);
        v3.Color = Color;
        v3.TextureCoordinate = FVector2D(0, 1/sqrt3);

        // TODO: set for each vertex
        //Vert.SetTangents(ForwardDir, OutDir ^ ForwardDir, OutDir);

        auto normal = FVector::CrossProduct(v2.Position - v1.Position, v3.Position - v1.Position).GetSafeNormal();


        /*auto surfaceTangent = ((((v3.TextureCoordinate.X - v1.TextureCoordinate.X) / FVector2D::Distance(v3.TextureCoordinate, v1.TextureCoordinate))*(v3.Position - v1.Position)) +
            (((v3.TextureCoordinate.Y - v1.TextureCoordinate.Y) / FVector2D::Distance(v3.TextureCoordinate, v1.TextureCoordinate))*FVector::CrossProduct(normal, (v3Position - v1.Position)))).GetSafeNormal();


        tangents->Add(FProcMeshTangent(surfaceTangent, true));
        tangents->Add(FProcMeshTangent(surfaceTangent, true));
        tangents->Add(FProcMeshTangent(surfaceTangent, true));*/
        const FVector Edge01 = (v2.Position - v1.Position);
        const FVector Edge02 = (v3.Position - v1.Position);

        const FVector TangentX = Edge01.GetSafeNormal();
        const FVector TangentZ = (Edge02 ^ Edge01).GetSafeNormal();
        const FVector TangentY = (TangentX ^ TangentZ).GetSafeNormal();
        v1.SetTangents(TangentX, TangentY, TangentZ);
        v2.SetTangents(TangentX, TangentY, TangentZ);
        v3.SetTangents(TangentX, TangentY, TangentZ);

        TArray<FDynamicMeshVertex> vertices;
        vertices.Add(v3);
        vertices.Add(v2);
        vertices.Add(v1);
        return vertices;
    }


    void BuildPointCloudMesh(const TArray<FVector>& InPoints, TArray<FDynamicMeshVertex>& OutVertices, TArray<int32>& OutIndices)
	{
		const FColor VertexColor(255,255,255);
		const int32 NPoints = InPoints.Num();
        check(NPoints == NumPoints);

        //check(Component->NumPoints == NumPoints);

		// Build vertices
        const FColor Color(255, 0,0);

		for(int32 PointIdx=0; PointIdx<NPoints; PointIdx++)
		{
            auto tri = triangle(InPoints[PointIdx], Color, triangleSize);
            OutVertices.Add(tri[0]);
            OutVertices.Add(tri[1]);
            OutVertices.Add(tri[2]);
            OutIndices.Add(PointIdx * 3 + 0);
            OutIndices.Add(PointIdx * 3 + 1);
            OutIndices.Add(PointIdx * 3 + 2);
		}
	}

	/** Called on render thread to assign new dynamic data */
	void SetDynamicData_RenderThread(FPointCloudDynamicData* NewDynamicData)
	{
		check(IsInRenderingThread());

		// Free existing data if present
		if(DynamicData)
		{
			delete DynamicData;
			DynamicData = NULL;
		}
		DynamicData = NewDynamicData;

		// Build mesh from PointCloud points
		TArray<FDynamicMeshVertex> Vertices;
		TArray<int32> Indices;
		BuildPointCloudMesh(NewDynamicData->PointCloudPoints, Vertices, Indices);

		check(Vertices.Num() == GetRequiredVertexCount());
		check(Indices.Num() == GetRequiredIndexCount());

		void* VertexBufferData = RHILockVertexBuffer(VertexBuffer.VertexBufferRHI, 0, Vertices.Num() * sizeof(FDynamicMeshVertex), RLM_WriteOnly);
		FMemory::Memcpy(VertexBufferData, &Vertices[0], Vertices.Num() * sizeof(FDynamicMeshVertex));
		RHIUnlockVertexBuffer(VertexBuffer.VertexBufferRHI);

		void* IndexBufferData = RHILockIndexBuffer(IndexBuffer.IndexBufferRHI, 0, Indices.Num() * sizeof(int32), RLM_WriteOnly);
		FMemory::Memcpy(IndexBufferData, &Indices[0], Indices.Num() * sizeof(int32));
		RHIUnlockIndexBuffer(IndexBuffer.IndexBufferRHI);
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_PointCloudSceneProxy_GetDynamicMeshElements );

		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

		auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
				GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy(IsSelected()) : NULL,
				FLinearColor(0, 0.5f, 1.f)
		);

		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);

		FMaterialRenderProxy* MaterialProxy = NULL;
		if(bWireframe)
		{
			MaterialProxy = WireframeMaterialInstance;
		}
		else
		{
			MaterialProxy = Material->GetRenderProxy(IsSelected());
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				// Draw the mesh.
				FMeshBatch& Mesh = Collector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.IndexBuffer = &IndexBuffer;
				Mesh.bWireframe = bWireframe;
				Mesh.VertexFactory = &VertexFactory;
				Mesh.MaterialRenderProxy = MaterialProxy;
				BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest());
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = GetRequiredIndexCount() / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = GetRequiredVertexCount();
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				Collector.AddMesh(ViewIndex, Mesh);

                RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				// Render bounds
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
#endif
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }

	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:

	UMaterialInterface* Material;
	FPointCloudVertexBuffer VertexBuffer;
	FPointCloudIndexBuffer IndexBuffer;
	FPointCloudVertexFactory VertexFactory;

	FPointCloudDynamicData* DynamicData;

	FMaterialRelevance MaterialRelevance;
	float TileMaterial;

	int32 NumPoints;
	int32 NumSides;
	float PointCloudWidth;
    float triangleSize;

    UPointCloudComponent* mComponent;
};



//////////////////////////////////////////////////////////////////////////

UPointCloudComponent::UPointCloudComponent( const FObjectInitializer& ObjectInitializer )
		: Super( ObjectInitializer )
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bAutoActivate = true;
    bWantsInitializeComponent = true;

    //bEnableAutoLODGeneration = false;

	PointCloudWidth = 10.f;
	//NumPoints = 512*512;
    triangleSize = 5.f;

    //SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
    SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
    SetSimulatePhysics(false);
}

FPrimitiveSceneProxy* UPointCloudComponent::CreateSceneProxy()
{
    auto sceneProxy = new FPointCloudSceneProxy(this);
    //UE_LOG(LogTemp, Warning, TEXT("created SceneProxy: %d"), sceneProxy);
	return sceneProxy;
}

int32 UPointCloudComponent::GetNumMaterials() const
{
	return 1;
}

void UPointCloudComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	for (int32 ElementIndex = 0; ElementIndex < GetNumMaterials(); ElementIndex++)
	{
		if (UMaterialInterface* MaterialInterface = GetMaterial(ElementIndex))
		{
			OutMaterials.Add(MaterialInterface);
		}
	}
}

void UPointCloudComponent::InitializeComponent()
{
    Super::InitializeComponent();
    //setNumPoints(NumPoints);
}

void UPointCloudComponent::OnRegister()
{
	Super::OnRegister();
    setNumPoints(NumPoints);
}

void UPointCloudComponent::setNumPoints(int numPoints) {
    NumPoints = numPoints;

    //UE_LOG(LogTemp, Warning, TEXT("mySceneProxy: %d"), SceneProxy);

    Points.Reset();
    Points.AddUninitialized(NumPoints);

	// Build up primitive mesh stack
    for (int32 ParticleIdx = 0; ParticleIdx<NumPoints; ParticleIdx++)
    {
        FPointCloudParticle& Particle = Points[ParticleIdx];
        const FVector InitialPosition = FVector(0, 0, ParticleIdx * 3);

        Particle.Position = InitialPosition;
    }
}


void UPointCloudComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
};

void UPointCloudComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

	SendRenderDynamicData_Concurrent();
}

void UPointCloudComponent::SendRenderDynamicData_Concurrent()
{
	if(SceneProxy)
	{
		// Allocate PointCloud dynamic data
		FPointCloudDynamicData* DynamicData = new FPointCloudDynamicData;

		// Transform current positions from particles into component-space array
		const FTransform& ComponentTransform = GetComponentTransform();
		DynamicData->PointCloudPoints.AddUninitialized(NumPoints);
        // TODO can we use memcopy or use only one buffer here?
		for(int32 PointIdx=0; PointIdx<NumPoints; PointIdx++)
		{
            //DynamicData->PointCloudPoints[PointIdx] = ComponentTransform.InverseTransformPosition(Points[PointIdx].Position);
            DynamicData->PointCloudPoints[PointIdx] = Points[PointIdx].Position;
		}

		// Enqueue command to send to render thread
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				FSendPointCloudDynamicData,
				FPointCloudSceneProxy*,PointCloudSceneProxy,(FPointCloudSceneProxy*)SceneProxy,
				FPointCloudDynamicData*,DynamicData,DynamicData,
				{
						PointCloudSceneProxy->SetDynamicData_RenderThread(DynamicData);
				});
	}
}

FBoxSphereBounds UPointCloudComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	//#ToDo

//	// Calculate bounding box of PointCloud points
//	FBox PointCloudBox(ForceInit);
//	for(int32 ParticleIdx=0; ParticleIdx<Points.Num(); ParticleIdx++)
//	{
//		const FPointCloudParticle& Particle = Points[ParticleIdx];
//		PointCloudBox += Particle.Position;
//	}
//
//	// Expand by PointCloud radius (half PointCloud width)
//	return FBoxSphereBounds(PointCloudBox.ExpandBy(0.5f * PointCloudWidth));

    int32 EdgeSize = INT_MAX;
    FVector cube(EdgeSize, EdgeSize, EdgeSize);

    FBoxSphereBounds NewBounds;
    NewBounds.Origin = LocalToWorld.GetLocation() + cube / 2;
    NewBounds.BoxExtent = cube;
    NewBounds.SphereRadius = FGenericPlatformMath::Sqrt(3 * EdgeSize);
    return NewBounds;
}
