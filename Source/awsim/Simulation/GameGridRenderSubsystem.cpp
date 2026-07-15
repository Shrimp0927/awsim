#include "Simulation/GameGridRenderSubsystem.h"
#include "Simulation/GameGridSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	struct FGridLayerStyle
	{
		const TCHAR* Name;
		FLinearColor Color;
		float Height;  // in tiles: 1 = one cube (TileSize) tall
		float Inset;   // XY shrink so neighbours read as separate boxes
	};

	// Connectors first, then buildings addressed by EDomain value (pure
	// consumers fall under None).
	constexpr int32 RoadLayer = 0;
	constexpr int32 UtilEnergyLayer = 1;
	constexpr int32 UtilWaterLayer = 2;
	constexpr int32 BuildingLayerBase = 3;
	constexpr int32 GroundLayer = 8; // after the EDomain-indexed building layers

	const FGridLayerStyle GridLayerStyles[] =
	{
		{ TEXT("Roads"),            FLinearColor(0.15f, 0.15f, 0.15f), 0.05f, 0.98f },
		{ TEXT("PowerLines"),       FLinearColor(0.9f, 0.5f, 0.1f),    0.08f, 0.5f },
		{ TEXT("Pipes"),            FLinearColor(0.1f, 0.6f, 0.8f),    0.08f, 0.5f },
		{ TEXT("Buildings"),        FLinearColor(0.55f, 0.55f, 0.55f), 1.0f,  0.9f }, // EDomain::None
		{ TEXT("HousingBuildings"), FLinearColor(0.2f, 0.75f, 0.25f),  1.0f,  0.9f },
		{ TEXT("EconomyBuildings"), FLinearColor(0.25f, 0.4f, 0.95f),  1.3f,  0.9f },
		{ TEXT("EnergyBuildings"),  FLinearColor(1.0f, 0.55f, 0.1f),   1.6f,  0.9f },
		{ TEXT("WaterBuildings"),   FLinearColor(0.1f, 0.7f, 0.9f),    1.6f,  0.9f },
		{ TEXT("Ground"),           FLinearColor(0.07f, 0.11f, 0.07f), 1.f,   1.f },
	};
	constexpr int32 NumGridLayers = UE_ARRAY_COUNT(GridLayerStyles);

	EDomain DominantProducedDomain(const UPlaceableDef* Def)
	{
		if (!Def) return EDomain::None;
		TMap<EDomain, float> Totals;
		float Best = 0.f;
		EDomain BestDomain = EDomain::None;
		for (const FSliderDef& Slider : Def->Sliders)
		{
			for (const FDomainEffect& Effect : Slider.Effects)
			{
				const float Produced = FMath::Max(0.f, FMath::Max(Effect.AmountAtMin, Effect.AmountAtMax));
				if (Produced <= 0.f) continue;
				float& Total = Totals.FindOrAdd(Effect.Domain);
				Total += Produced;
				if (Total > Best)
				{
					Best = Total;
					BestDomain = Effect.Domain;
				}
			}
		}
		return BestDomain;
	}
}

void UGridRenderSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Engine primitives as stand-in art; real meshes come from UPlaceableDef::Mesh later.
	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
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

	Layers.SetNum(NumGridLayers);
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
	// RenderActor only exists in worlds that began play, keeping the tick out
	// of editor-preview worlds.
	return RenderActor != nullptr;
}

TStatId UGridRenderSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGridRenderSubsystem, STATGROUP_Tickables);
}

UInstancedStaticMeshComponent* UGridRenderSubsystem::EnsureLayer(int32 LayerIndex)
{
	if (Layers[LayerIndex])
	{
		return Layers[LayerIndex];
	}

	const FGridLayerStyle& Style = GridLayerStyles[LayerIndex];
	UInstancedStaticMeshComponent* Layer = NewObject<UInstancedStaticMeshComponent>(RenderActor, Style.Name);
	Layer->SetStaticMesh(CubeMesh);
	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(BaseMaterial, Layer);
		Mid->SetVectorParameterValue(TEXT("Color"), Style.Color);
		Layer->SetMaterial(0, Mid);
	}
	Layer->SetMobility(EComponentMobility::Movable);
	Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision); // picking comes with the camera/input pass
	Layer->SetupAttachment(RenderActor->GetRootComponent());
	Layer->RegisterComponent();

	Layers[LayerIndex] = Layer;
	return Layer;
}

void UGridRenderSubsystem::RebuildInstances(const UGridSubsystem& Grid)
{
	for (UInstancedStaticMeshComponent* Layer : Layers)
	{
		if (Layer)
		{
			Layer->ClearInstances();
		}
	}

	// The engine cube is TileSize (100) units, pivot centered, so scale is in
	// tiles and Z sits at half the box height.
	auto AddBox = [this](int32 LayerIndex, FGridCoord Origin, FIntPoint ExtentTiles)
	{
		constexpr float TileSize = UGridSubsystem::TileSize;
		const FGridLayerStyle& Style = GridLayerStyles[LayerIndex];
		const FVector Center(
			(Origin.X + ExtentTiles.X * 0.5f) * TileSize,
			(Origin.Y + ExtentTiles.Y * 0.5f) * TileSize,
			Style.Height * TileSize * 0.5f);
		const FVector Scale(ExtentTiles.X * Style.Inset, ExtentTiles.Y * Style.Inset, Style.Height);
		EnsureLayer(LayerIndex)->AddInstance(FTransform(FQuat::Identity, Center, Scale));
	};

	// Ground slab top sits slightly above z = 0 so it cannot z-fight a map
	// floor at zero; buildings sink imperceptibly into it.
	{
		constexpr float TileSize = UGridSubsystem::TileSize;
		constexpr float TopZ = 2.f;
		constexpr float Thickness = 10.f;
		const FVector Center(
			UGridSubsystem::GetWidth() * TileSize * 0.5f,
			UGridSubsystem::GetHeight() * TileSize * 0.5f,
			TopZ - Thickness * 0.5f);
		const FVector Scale(UGridSubsystem::GetWidth(), UGridSubsystem::GetHeight(), Thickness / TileSize);
		EnsureLayer(GroundLayer)->AddInstance(FTransform(FQuat::Identity, Center, Scale));
	}

	for (const FPlacedBuilding& Building : Grid.GetBuildings())
	{
		const EDomain Domain = DominantProducedDomain(Building.Content.Definition);
		AddBox(BuildingLayerBase + static_cast<int32>(Domain),
			Building.Origin, UGridSubsystem::GetFootprintExtent(Building.Content));
	}
	for (const TPair<FGridCoord, FGridContent>& Road : Grid.GetRoads())
	{
		AddBox(RoadLayer, Road.Key, FIntPoint(1, 1));
	}
	for (const TPair<FGridCoord, FGridContent>& Util : Grid.GetUtilities())
	{
		const EDomain Domain = Util.Value.Definition ? Util.Value.Definition->ConnectorDomain : EDomain::None;
		AddBox(Domain == EDomain::Water ? UtilWaterLayer : UtilEnergyLayer, Util.Key, FIntPoint(1, 1));
	}
}
