#include "Simulation/GameGridRenderSubsystem.h"
#include "Simulation/GameGridSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	FQuat FacingRotation(EPlaceableDirection Facing)
	{
		switch (Facing)
		{
		case EPlaceableDirection::East:  return FRotator(0.f, 90.f, 0.f).Quaternion();
		case EPlaceableDirection::South: return FRotator(0.f, 180.f, 0.f).Quaternion();
		case EPlaceableDirection::West:  return FRotator(0.f, 270.f, 0.f).Quaternion();
		default:                         return FQuat::Identity;
		}
	}
}

void UGridRenderSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("GridRender: engine cube mesh not found; grid will not be drawn."));
		return;
	}

	FActorSpawnParameters Params;
	Params.Name = TEXT("AwsimGridRenderer");
	Params.ObjectFlags = RF_Transient;
	RenderActor = InWorld.SpawnActor<AActor>(Params);

	USceneComponent* Root = NewObject<USceneComponent>(RenderActor, TEXT("Root"));
	RenderActor->SetRootComponent(Root);
	Root->RegisterComponent();

	MissingMeshLayer = CreateIsmComponent(CubeMesh);

	// Placed once; top sits slightly above z = 0 to avoid z-fighting a map floor.
	Ground = CreateIsmComponent(CubeMesh);
	if (UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(BaseMaterial, Ground);
		Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.07f, 0.11f, 0.07f));
		Ground->SetMaterial(0, Mid);
	}
	constexpr float TileSize = UGridSubsystem::TileSize;
	constexpr float TopZ = 2.f;
	constexpr float Thickness = 10.f;
	const FVector Center(0.f, 0.f, TopZ - Thickness * 0.5f); // grid is centered on the world origin
	const FVector Scale(UGridSubsystem::GetWidth(), UGridSubsystem::GetHeight(), Thickness / TileSize);
	Ground->AddInstance(FTransform(FQuat::Identity, Center, Scale));
}

void UGridRenderSubsystem::Tick(float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	const UGridSubsystem* Grid = World ? World->GetSubsystem<UGridSubsystem>() : nullptr;
	if (!Grid)
	{
		return;
	}

	const uint64 Revision = Grid->GetContentRevision();
	if (Revision != LastRevision)
	{
		LastRevision = Revision;
		RebuildInstances(*Grid);
	}
}

bool UGridRenderSubsystem::IsTickable() const
{
	// RenderActor gates the tick out of editor-preview worlds.
	return RenderActor != nullptr;
}

TStatId UGridRenderSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGridRenderSubsystem, STATGROUP_Tickables);
}

UInstancedStaticMeshComponent* UGridRenderSubsystem::CreateIsmComponent(UStaticMesh* Mesh)
{
	UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(RenderActor);
	Ism->SetStaticMesh(Mesh);
	Ism->SetMobility(EComponentMobility::Movable);
	Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision); // picking comes with the input pass
	Ism->SetupAttachment(RenderActor->GetRootComponent());
	Ism->RegisterComponent();
	return Ism;
}

UInstancedStaticMeshComponent* UGridRenderSubsystem::EnsureMeshLayer(UStaticMesh* Mesh)
{
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = MeshLayers.Find(Mesh))
	{
		return *Found;
	}

	UInstancedStaticMeshComponent* Layer = CreateIsmComponent(Mesh);
	MeshLayers.Add(Mesh, Layer);
	return Layer;
}

void UGridRenderSubsystem::RebuildInstances(const UGridSubsystem& Grid)
{
	for (const TPair<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>>& Pair : MeshLayers)
	{
		if (Pair.Value)
		{
			Pair.Value->ClearInstances();
		}
	}
	if (MissingMeshLayer)
	{
		MissingMeshLayer->ClearInstances();
	}

	constexpr float TileSize = UGridSubsystem::TileSize;

	// Def meshes are authored at world scale, pivot at the footprint's base
	// center; mesh-less defs render as footprint-sized cubes.
	auto AddContent = [&](const FGridCoord& Origin, const FGridContent& Content)
	{
		const FIntPoint Ext = UGridSubsystem::GetFootprintExtent(Content);
		const FVector BaseCenter(
			UGridSubsystem::WorldMinX + (Origin.X + Ext.X * 0.5f) * TileSize,
			UGridSubsystem::WorldMinY + (Origin.Y + Ext.Y * 0.5f) * TileSize,
			0.f);

		const UPlaceableDef* Def = Content.Definition;
		if (UStaticMesh* Mesh = Def ? Def->Mesh.LoadSynchronous() : nullptr)
		{
			EnsureMeshLayer(Mesh)->AddInstance(
				FTransform(FacingRotation(Content.Facing), BaseCenter, FVector::OneVector));
			return;
		}

		// Under ~20 tiles is sub-pixel from the whole-grid camera; the engine
		// cube's pivot is centered, so lift by half its height.
		constexpr float FallbackHeightTiles = 20.f;
		const float HeightTiles = Content.Type == EPlaceableType::Building
			? FallbackHeightTiles * 5.f : FallbackHeightTiles;
		MissingMeshLayer->AddInstance(FTransform(
			FQuat::Identity,
			BaseCenter + FVector(0.f, 0.f, TileSize * HeightTiles * 0.5f),
			FVector(Ext.X, Ext.Y, HeightTiles)));
	};

	for (const FPlacedBuilding& Building : Grid.GetBuildings())
	{
		AddContent(Building.Origin, Building.Content);
	}
	for (const TPair<FGridCoord, FGridContent>& Road : Grid.GetRoads())
	{
		AddContent(Road.Key, Road.Value);
	}
	for (const TPair<FGridCoord, FGridContent>& Util : Grid.GetUtilities())
	{
		AddContent(Util.Key, Util.Value);
	}
}
