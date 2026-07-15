#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GameGridRenderSubsystem.generated.h"

class AActor;
class UGridSubsystem;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

// Projects grid content into instanced static meshes. Deliberately NOT a
// USimPhase: it ticks per frame and rebuilds only when the grid's content
// revision changes.
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
	UInstancedStaticMeshComponent* EnsureLayer(int32 LayerIndex);
	void RebuildInstances(const UGridSubsystem& Grid);

	// One ISM layer per visual category (roads, utilities, buildings by their
	// dominant domain).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Layers;

	UPROPERTY(Transient) TObjectPtr<AActor> RenderActor;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BaseMaterial;

	// MAX forces the first build (revision 0 = empty grid still needs its
	// initial clear).
	uint64 LastRevision = MAX_uint64;
};
