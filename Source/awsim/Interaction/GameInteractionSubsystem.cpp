#include "Interaction/GameInteractionSubsystem.h"
#include "Simulation/GameGridSubsystem.h"
#include "awsim.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"

void UGameInteractionSubsystem::Tick(float DeltaSeconds)
{
	bHasHover = false;

	const UWorld* World = GetWorld();
	const UGridSubsystem* Grid = World ? World->GetSubsystem<UGridSubsystem>() : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	EnsureInputBound(PC);
	bHasGroundHover = false;
	FVector Origin, Dir;
	if (Grid && PC && PC->DeprojectMousePositionToWorld(Origin, Dir))
	{
		bHasHover = Grid->PickTile(Origin, Dir, HoveredTile);
		bHasGroundHover = Grid->PickGroundTile(Origin, Dir, GroundHoveredTile);
	}
	if (bDragging && bHasGroundHover)
	{
		DragEndTile = GroundHoveredTile;
	}
}

FIntRect UGameInteractionSubsystem::GetDragRect() const
{
	return FIntRect(
		FMath::Min(AnchorTile.X, DragEndTile.X), FMath::Min(AnchorTile.Y, DragEndTile.Y),
		FMath::Max(AnchorTile.X, DragEndTile.X) + 1, FMath::Max(AnchorTile.Y, DragEndTile.Y) + 1);
}

void UGameInteractionSubsystem::EnsureInputBound(APlayerController* PC)
{
	if (bInputBound || !PC)
	{
		return;
	}
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PC->InputComponent);
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!Input || !Subsystem)
	{
		return;
	}

	DemolishAction = NewObject<UInputAction>(this);
	DemolishAction->ValueType = EInputActionValueType::Boolean;
	SelectAction = NewObject<UInputAction>(this);
	SelectAction->ValueType = EInputActionValueType::Boolean;
	UInputMappingContext* Context = NewObject<UInputMappingContext>(this);
	Context->MapKey(DemolishAction, EKeys::X);
	Context->MapKey(SelectAction, EKeys::LeftMouseButton);
	Input->BindAction(DemolishAction, ETriggerEvent::Started, this, &UGameInteractionSubsystem::OnDemolish);
	Input->BindAction(SelectAction, ETriggerEvent::Started, this, &UGameInteractionSubsystem::OnSelectPressed);
	Input->BindAction(SelectAction, ETriggerEvent::Completed, this, &UGameInteractionSubsystem::OnSelectReleased);
	Subsystem->AddMappingContext(Context, 0);
	bInputBound = true;
}

void UGameInteractionSubsystem::OnSelectPressed(const FInputActionValue& Value)
{
	if (bHasGroundHover)
	{
		bDragging = true;
		AnchorTile = GroundHoveredTile;
		DragEndTile = GroundHoveredTile;
	}
}

void UGameInteractionSubsystem::OnSelectReleased(const FInputActionValue& Value)
{
	bDragging = false;
}

void UGameInteractionSubsystem::OnDemolish(const FInputActionValue& Value)
{
	UGridSubsystem* Grid = GetWorld() ? GetWorld()->GetSubsystem<UGridSubsystem>() : nullptr;
	if (!Grid)
	{
		return;
	}

	if (bDragging)
	{
		// Buildings dedupe to their origin so a partially covered one is removed whole.
		TSet<FGridCoord> Targets;
		const FIntRect Rect = GetDragRect();
		for (int32 y = Rect.Min.Y; y < Rect.Max.Y; ++y)
		{
			for (int32 x = Rect.Min.X; x < Rect.Max.X; ++x)
			{
				const FGridCoord Tile(x, y);
				if (const FPlacedBuilding* Building = Grid->FindBuildingAt(Tile))
				{
					Targets.Add(Building->Origin);
				}
				else if (Grid->IsTileOccupied(Tile))
				{
					Targets.Add(Tile);
				}
			}
		}
		for (const FGridCoord& Target : Targets)
		{
			Grid->QueuePlacement(Target, FGridContent());
		}
		UE_LOG(LogAwsim, Log, TEXT("Queued %d demolition(s) in (%d, %d)..(%d, %d)."),
			Targets.Num(), Rect.Min.X, Rect.Min.Y, Rect.Max.X - 1, Rect.Max.Y - 1);
		return;
	}

	if (!bHasHover)
	{
		return;
	}
	Grid->QueuePlacement(HoveredTile, FGridContent()); // demolition intent, applies on the next sim step
	UE_LOG(LogAwsim, Log, TEXT("Queued demolition at (%d, %d)."), HoveredTile.X, HoveredTile.Y);
}

bool UGameInteractionSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->IsGameWorld();
}

TStatId UGameInteractionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGameInteractionSubsystem, STATGROUP_Tickables);
}
