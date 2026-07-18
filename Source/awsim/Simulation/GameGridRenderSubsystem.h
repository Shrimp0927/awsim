#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GameGridRenderSubsystem.generated.h"

class AActor;
class UGridSubsystem;
class UInstancedStaticMeshComponent;
class UStaticMesh;

// Projects grid content into ISMs. Deliberately NOT a USimPhase: it ticks per
// frame and rebuilds only when the grid's content revision changes.
UCLASS()
class AWSIM_API UGridRenderSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

private:
	UInstancedStaticMeshComponent* CreateIsmComponent(UStaticMesh* Mesh);
	UInstancedStaticMeshComponent* EnsureMeshLayer(UStaticMesh* Mesh);
	void RebuildInstances(const UGridSubsystem& Grid);

	// One ISM per unique def mesh; content renders whatever its def authors.
	UPROPERTY(Transient)
	TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>> MeshLayers;

	// Footprint-sized cubes for defs with no authored mesh.
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> MissingMeshLayer;

	// Stand-in flat world floor until a real map exists.
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Ground;

	UPROPERTY(Transient) TObjectPtr<AActor> RenderActor;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CubeMesh;

	// MAX forces the first build.
	uint64 LastRevision = MAX_uint64;
};
