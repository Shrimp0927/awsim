#include "Interaction/GameInteractionSubsystem.h"
#include "Core/GameFlowSubsystem.h"
#include "Simulation/GameGridSubsystem.h"
#include "awsim.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// Per-tile rates mirroring the dev defs; a real def catalog replaces this later.
	UPlaceableDef* MakeDragDef(EDomain Domain, FIntPoint Dims)
	{
		float CostPerTile = 3.f, MaintenancePerTile = 0.05f, Height = 20.f;
		TArray<TPair<EDomain, float>> Effects;
		switch (Domain)
		{
		case EDomain::Energy:
			Effects = {{EDomain::Energy, 2.f}};
			CostPerTile = 5.f; MaintenancePerTile = 0.2f; Height = 12.f;
			break;
		case EDomain::Water:
			Effects = {{EDomain::Water, 2.f}};
			CostPerTile = 5.f; MaintenancePerTile = 0.15f; Height = 10.f;
			break;
		case EDomain::Housing:
			Effects = {{EDomain::Housing, 0.5f}, {EDomain::Energy, -0.1f}, {EDomain::Water, -0.1f}};
			break;
		case EDomain::Economy:
			Effects = {{EDomain::Economy, 1.f}, {EDomain::Energy, -0.2f}};
			MaintenancePerTile = 0.08f; Height = 45.f;
			break;
		default:
			return nullptr;
		}

		UPlaceableDef* Def = NewObject<UPlaceableDef>(GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UPlaceableDef::StaticClass(), TEXT("DragBuilding")));
		Def->Type = EPlaceableType::Building;
		Def->Dimensions = Dims;
		Def->HeightTiles = Height;
		const float Area = Dims.X * Dims.Y;
		Def->Cost = CostPerTile * Area;
		Def->DailyMaintenanceCost = MaintenancePerTile * Area;
		FSliderDef Slider;
		Slider.Name = TEXT("Operation");
		for (const TPair<EDomain, float>& E : Effects)
		{
			FDomainEffect Effect;
			Effect.Domain = E.Key;
			Effect.AmountAtMin = E.Value * 0.5f;
			Effect.AmountAtMax = E.Value;
			Slider.Effects.Add(Effect);
		}
		Def->Sliders.Add(Slider);
		return Def;
	}
}

void UGameInteractionSubsystem::Tick(float DeltaSeconds)
{
	bHasHover = false;

	const UWorld* World = GetWorld();
	const UGridSubsystem* Grid = World ? World->GetSubsystem<UGridSubsystem>() : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	EnsureInputBound(PC);
	bHasGroundHover = false;

	// The menu swallows the world cursor entirely.
	const UGameFlowSubsystem* Flow = World ? World->GetSubsystem<UGameFlowSubsystem>() : nullptr;
	if (Flow && Flow->GetState() == EGameFlowState::Menu)
	{
		bDragging = false;
		return;
	}

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

	// 1 energy, 2 water, 3 residential, 4 business, 5 road, 6 power line, 7 pipe.
	const FKey BuildKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven};
	using FBuildHandler = void (UGameInteractionSubsystem::*)(const FInputActionValue&);
	const FBuildHandler BuildHandlers[] = {
		&UGameInteractionSubsystem::OnBuildEnergy, &UGameInteractionSubsystem::OnBuildWater,
		&UGameInteractionSubsystem::OnBuildHome, &UGameInteractionSubsystem::OnBuildBusiness,
		&UGameInteractionSubsystem::OnBuildRoad, &UGameInteractionSubsystem::OnBuildPowerLine,
		&UGameInteractionSubsystem::OnBuildPipe};
	BuildActions.SetNum(UE_ARRAY_COUNT(BuildKeys));
	for (int32 i = 0; i < BuildActions.Num(); ++i)
	{
		BuildActions[i] = NewObject<UInputAction>(this);
		BuildActions[i]->ValueType = EInputActionValueType::Boolean;
		Context->MapKey(BuildActions[i], BuildKeys[i]);
		Input->BindAction(BuildActions[i], ETriggerEvent::Started, this, BuildHandlers[i]);
	}

	// Escape works in packaged builds; M is the PIE-friendly twin (PIE eats Escape).
	ToggleMenuAction = NewObject<UInputAction>(this);
	ToggleMenuAction->ValueType = EInputActionValueType::Boolean;
	Context->MapKey(ToggleMenuAction, EKeys::Escape);
	Context->MapKey(ToggleMenuAction, EKeys::M);
	Input->BindAction(ToggleMenuAction, ETriggerEvent::Started, this, &UGameInteractionSubsystem::OnToggleMenu);

	Subsystem->AddMappingContext(Context, 0);
	bInputBound = true;
}

void UGameInteractionSubsystem::OnToggleMenu(const FInputActionValue& Value)
{
	if (UGameFlowSubsystem* Flow = GetWorld() ? GetWorld()->GetSubsystem<UGameFlowSubsystem>() : nullptr)
	{
		Flow->ToggleMenu();
	}
}

void UGameInteractionSubsystem::OnBuildEnergy(const FInputActionValue& Value)    { QueueBuildInRect(EDomain::Energy); }
void UGameInteractionSubsystem::OnBuildWater(const FInputActionValue& Value)     { QueueBuildInRect(EDomain::Water); }
void UGameInteractionSubsystem::OnBuildHome(const FInputActionValue& Value)      { QueueBuildInRect(EDomain::Housing); }
void UGameInteractionSubsystem::OnBuildBusiness(const FInputActionValue& Value)  { QueueBuildInRect(EDomain::Economy); }
void UGameInteractionSubsystem::OnBuildRoad(const FInputActionValue& Value)      { QueueConnectorsInRect(EPlaceableType::Road, EDomain::None); }
void UGameInteractionSubsystem::OnBuildPowerLine(const FInputActionValue& Value) { QueueConnectorsInRect(EPlaceableType::Utility, EDomain::Energy); }
void UGameInteractionSubsystem::OnBuildPipe(const FInputActionValue& Value)      { QueueConnectorsInRect(EPlaceableType::Utility, EDomain::Water); }

void UGameInteractionSubsystem::QueueConnectorsInRect(EPlaceableType Type, EDomain Domain)
{
	UGridSubsystem* Grid = GetWorld() ? GetWorld()->GetSubsystem<UGridSubsystem>() : nullptr;
	if (!Grid || !bDragging)
	{
		return;
	}

	const FIntRect Rect = GetDragRect();
	for (int32 y = Rect.Min.Y; y < Rect.Max.Y; ++y)
	{
		for (int32 x = Rect.Min.X; x < Rect.Max.X; ++x)
		{
			if (Grid->IsTileOccupied(FGridCoord(x, y)))
			{
				UE_LOG(LogAwsim, Warning, TEXT("Build rejected: (%d, %d) is occupied."), x, y);
				return;
			}
		}
	}

	// One shared def; each tile is its own 1x1 placement.
	UPlaceableDef* Def = NewObject<UPlaceableDef>(GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UPlaceableDef::StaticClass(), TEXT("DragConnector")));
	Def->Type = Type;
	Def->Dimensions = FIntPoint(1, 1);
	Def->ConnectorDomain = Domain;
	Def->Cost = Type == EPlaceableType::Road ? 10.f : 8.f;

	for (int32 y = Rect.Min.Y; y < Rect.Max.Y; ++y)
	{
		for (int32 x = Rect.Min.X; x < Rect.Max.X; ++x)
		{
			FGridContent Content;
			Content.Type = Type;
			Content.Facing = EPlaceableDirection::None;
			Content.Definition = Def;
			Grid->QueuePlacement(FGridCoord(x, y), Content);
		}
	}
	UE_LOG(LogAwsim, Log, TEXT("Queued %d connector tile(s) at (%d, %d), cost %.0f."),
		Rect.Area(), Rect.Min.X, Rect.Min.Y, Def->Cost * Rect.Area());
}

void UGameInteractionSubsystem::QueueBuildInRect(EDomain Domain)
{
	UGridSubsystem* Grid = GetWorld() ? GetWorld()->GetSubsystem<UGridSubsystem>() : nullptr;
	if (!Grid || !bDragging)
	{
		return;
	}

	const FIntRect Rect = GetDragRect();
	for (int32 y = Rect.Min.Y; y < Rect.Max.Y; ++y)
	{
		for (int32 x = Rect.Min.X; x < Rect.Max.X; ++x)
		{
			if (Grid->IsTileOccupied(FGridCoord(x, y)))
			{
				UE_LOG(LogAwsim, Warning, TEXT("Build rejected: (%d, %d) is occupied."), x, y);
				return;
			}
		}
	}

	UPlaceableDef* Def = MakeDragDef(Domain, FIntPoint(Rect.Width(), Rect.Height()));
	if (!Def)
	{
		return;
	}
	FGridContent Content;
	Content.Type = EPlaceableType::Building;
	Content.Facing = EPlaceableDirection::North;
	Content.Definition = Def;
	Grid->QueuePlacement(FGridCoord(Rect.Min.X, Rect.Min.Y), Content);
	UE_LOG(LogAwsim, Log, TEXT("Queued %dx%d %s building at (%d, %d), cost %.0f."),
		Rect.Width(), Rect.Height(), *UEnum::GetDisplayValueAsText(Domain).ToString(),
		Rect.Min.X, Rect.Min.Y, Def->Cost);
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
