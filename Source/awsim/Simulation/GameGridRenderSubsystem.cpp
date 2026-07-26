#include "Simulation/GameGridRenderSubsystem.h"
#include "Simulation/GameGridSubsystem.h"
#include "Interaction/GameInteractionSubsystem.h"
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

	// Color category for mesh-less content; buildings take the domain they produce.
	enum class ECat : uint8 { Housing, Economy, Energy, Water, Road, Other, Count };
	constexpr int32 CatCount = static_cast<int32>(ECat::Count);

	ECat CategoryOf(const FGridContent& Content)
	{
		const UPlaceableDef* Def = Content.Definition;
		if (Content.Type == EPlaceableType::Road)
		{
			return ECat::Road;
		}
		if (Content.Type == EPlaceableType::Utility)
		{
			const EDomain Domain = Def ? Def->ConnectorDomain : EDomain::None;
			return Domain == EDomain::Energy ? ECat::Energy
				: Domain == EDomain::Water ? ECat::Water : ECat::Other;
		}
		if (DefProducesDomain(Def, EDomain::Housing)) return ECat::Housing;
		if (DefProducesDomain(Def, EDomain::Economy)) return ECat::Economy;
		if (DefProducesDomain(Def, EDomain::Energy))  return ECat::Energy;
		if (DefProducesDomain(Def, EDomain::Water))   return ECat::Water;
		return ECat::Other;
	}

	FLinearColor CategoryColor(ECat Cat)
	{
		switch (Cat)
		{
		case ECat::Housing: return FLinearColor(0.15f, 0.65f, 0.2f);
		case ECat::Economy: return FLinearColor(0.75f, 0.12f, 0.12f);
		case ECat::Energy:  return FLinearColor(0.95f, 0.8f, 0.1f);
		case ECat::Water:   return FLinearColor(0.12f, 0.35f, 0.85f);
		case ECat::Road:    return FLinearColor(0.5f, 0.2f, 0.75f);
		default:            return FLinearColor(0.55f, 0.55f, 0.55f);
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
	Root->SetMobility(EComponentMobility::Static); // grid renderer never moves; lets VSM cache shadow pages
	RenderActor->SetRootComponent(Root);
	Root->RegisterComponent();

	CategoryLayers.SetNum(CatCount);
	HoverLayers.SetNum(CatCount);
	for (int32 i = 0; i < CatCount; ++i)
	{
		const FLinearColor Color = CategoryColor(static_cast<ECat>(i));
		CategoryLayers[i] = CreateIsmComponent(CubeMesh, Color);
		HoverLayers[i] = CreateIsmComponent(CubeMesh, FMath::Lerp(Color, FLinearColor::White, 0.55f));
		// Shadowless so per-frame hover changes never invalidate shadow pages.
		HoverLayers[i]->SetCastShadow(false);
	}

	// Placed once; top sits slightly above z = 0 to avoid z-fighting a map floor.
	Ground = CreateIsmComponent(CubeMesh, FLinearColor(0.07f, 0.11f, 0.07f));
	Ground->SetCastShadow(false); // a flat floor casts nothing useful
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

	UpdateHighlight(*Grid);
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

UInstancedStaticMeshComponent* UGridRenderSubsystem::CreateIsmComponent(UStaticMesh* Mesh, TOptional<FLinearColor> Color)
{
	UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(RenderActor);
	Ism->SetStaticMesh(Mesh);
	// Static so VSM caches shadow pages; instance rebuilds still work.
	Ism->SetMobility(EComponentMobility::Static);
	Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision); // grid picking is analytic, no physics
	if (Color)
	{
		if (UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(BaseMaterial, Ism);
			Mid->SetVectorParameterValue(TEXT("Color"), *Color);
			Ism->SetMaterial(0, Mid);
		}
	}
	Ism->SetupAttachment(RenderActor->GetRootComponent());
	Ism->RegisterComponent();
	return Ism;
}

void UGridRenderSubsystem::UpdateHighlight(const UGridSubsystem& Grid)
{
	const UGameInteractionSubsystem* Interaction = GetWorld() ? GetWorld()->GetSubsystem<UGameInteractionSubsystem>() : nullptr;

	FIntRect Tiles(0, 0, 0, 0);
	int32 Cat = static_cast<int32>(ECat::Other);
	float HeightTiles = 0.f; // 0 = flat plate (bare ground / roads / utilities)
	if (Interaction && Interaction->IsDragSelecting())
	{
		Tiles = Interaction->GetDragRect(); // marquee plate; replaces the hover shell while held
	}
	else if (Interaction && Interaction->HasHoveredTile())
	{
		const FGridCoord Hovered = Interaction->GetHoveredTile();
		if (const FPlacedBuilding* Building = Grid.FindBuildingAt(Hovered))
		{
			const FIntPoint Ext = UGridSubsystem::GetFootprintExtent(Building->Content);
			Tiles = FIntRect(Building->Origin.X, Building->Origin.Y,
				Building->Origin.X + Ext.X, Building->Origin.Y + Ext.Y);
			Cat = static_cast<int32>(CategoryOf(Building->Content));
			HeightTiles = FMath::Max(Building->HeightTiles, 1);
		}
		else
		{
			Tiles = FIntRect(Hovered.X, Hovered.Y, Hovered.X + 1, Hovered.Y + 1);
		}
	}
	if (Tiles == LastHighlightTiles)
	{
		return;
	}
	LastHighlightTiles = Tiles;

	for (const TObjectPtr<UInstancedStaticMeshComponent>& Layer : HoverLayers)
	{
		if (Layer)
		{
			Layer->ClearInstances();
		}
	}
	if (Tiles.Area() == 0)
	{
		return;
	}

	constexpr float TileSize = UGridSubsystem::TileSize;
	const FVector2D Center(
		UGridSubsystem::WorldMinX + (Tiles.Min.X + Tiles.Max.X) * 0.5f * TileSize,
		UGridSubsystem::WorldMinY + (Tiles.Min.Y + Tiles.Max.Y) * 0.5f * TileSize);

	if (HeightTiles > 0.f)
	{
		// Inflated shell overdraws the building in a lighter shade of its color.
		const float Height = HeightTiles * 1.02f;
		HoverLayers[Cat]->AddInstance(FTransform(
			FQuat::Identity,
			FVector(Center, TileSize * Height * 0.5f),
			FVector(Tiles.Width() * 1.04f, Tiles.Height() * 1.04f, Height)));
	}
	else
	{
		constexpr float Thickness = 4.f;
		constexpr float TopZ = 4.f; // above the ground plate, below content
		HoverLayers[Cat]->AddInstance(FTransform(
			FQuat::Identity,
			FVector(Center, TopZ - Thickness * 0.5f),
			FVector(Tiles.Width(), Tiles.Height(), Thickness / TileSize)));
	}
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
	for (const TObjectPtr<UInstancedStaticMeshComponent>& Layer : CategoryLayers)
	{
		if (Layer)
		{
			Layer->ClearInstances();
		}
	}

	constexpr float TileSize = UGridSubsystem::TileSize;

	// Def meshes author world scale with a base-center pivot; mesh-less defs render as cubes.
	auto AddContent = [&](const FGridCoord& Origin, const FGridContent& Content, int32 GrownHeightTiles)
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

		UInstancedStaticMeshComponent* Layer = CategoryLayers[static_cast<int32>(CategoryOf(Content))];
		if (Content.Type == EPlaceableType::Building)
		{
			// Cube pivot is centered — lift by half height.
			const float HeightTiles = FMath::Max(GrownHeightTiles, 1);
			Layer->AddInstance(FTransform(
				FQuat::Identity,
				BaseCenter + FVector(0.f, 0.f, TileSize * HeightTiles * 0.5f),
				FVector(Ext.X, Ext.Y, HeightTiles)));
		}
		else
		{
			// Roads and utilities lie flat: a thin slab just above the ground plate.
			constexpr float Thickness = 2.f;
			constexpr float TopZ = 3.f;
			Layer->AddInstance(FTransform(
				FQuat::Identity,
				BaseCenter + FVector(0.f, 0.f, TopZ - Thickness * 0.5f),
				FVector(Ext.X, Ext.Y, Thickness / TileSize)));
		}
	};

	for (const FPlacedBuilding& Building : Grid.GetBuildings())
	{
		AddContent(Building.Origin, Building.Content, Building.HeightTiles);
	}
	for (const TPair<FGridCoord, FGridContent>& Road : Grid.GetRoads())
	{
		AddContent(Road.Key, Road.Value, 0);
	}
	for (const TPair<FGridCoord, FGridContent>& Util : Grid.GetUtilities())
	{
		AddContent(Util.Key, Util.Value, 0);
	}
}
