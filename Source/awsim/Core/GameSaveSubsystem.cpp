#include "Core/GameSaveSubsystem.h"
#include "awsim.h"
#include "Entities/GridContent.h"
#include "Simulation/GameCityStatsSubsystem.h"
#include "Simulation/GameEditSubsystem.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GamePlayerFundsSubsystem.h"
#include "Simulation/GamePopulationSubsystem.h"
#include "Simulation/GameSimulationSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
	constexpr uint32 SaveMagic = 0x4157534D; // 'AWSM'

	// Defs are transient (no catalog yet): a deduplicated table, referenced by index.
	void SerializeDef(FArchive& Ar, UPlaceableDef& Def)
	{
		uint8 Type = static_cast<uint8>(Def.Type);
		uint8 ConnectorDomain = static_cast<uint8>(Def.ConnectorDomain);
		FString MeshPath = Ar.IsSaving() ? Def.Mesh.ToSoftObjectPath().ToString() : FString();
		Ar << Type << ConnectorDomain << MeshPath;
		Ar << Def.Cost << Def.DailyMaintenanceCost << Def.Dimensions << Def.HeightTiles;

		int32 NumSliders = Def.Sliders.Num();
		Ar << NumSliders;
		if (Ar.IsLoading())
		{
			Def.Type = static_cast<EPlaceableType>(Type);
			Def.ConnectorDomain = static_cast<EDomain>(ConnectorDomain);
			Def.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
			Def.Sliders.SetNum(NumSliders);
		}
		for (int32 s = 0; s < NumSliders; ++s)
		{
			FSliderDef& Slider = Def.Sliders[s];
			FString Name = Slider.Name.ToString();
			Ar << Name << Slider.Range.Min << Slider.Range.Max << Slider.Value;
			if (Ar.IsLoading())
			{
				Slider.Name = FName(*Name);
			}
			int32 NumEffects = Slider.Effects.Num();
			Ar << NumEffects;
			if (Ar.IsLoading())
			{
				Slider.Effects.SetNum(NumEffects);
			}
			for (int32 e = 0; e < NumEffects; ++e)
			{
				FDomainEffect& Effect = Slider.Effects[e];
				uint8 Domain = static_cast<uint8>(Effect.Domain);
				Ar << Domain << Effect.AmountAtMin << Effect.AmountAtMax;
				if (Ar.IsLoading())
				{
					Effect.Domain = static_cast<EDomain>(Domain);
				}
			}
		}
	}

	// Everything of FGridContent except the def pointer, carried as an index.
	void SerializeContent(FArchive& Ar, FGridContent& Content, int32& DefIndex)
	{
		uint8 Type = static_cast<uint8>(Content.Type);
		uint8 Facing = static_cast<uint8>(Content.Facing);
		Ar << DefIndex << Type << Facing << Content.SliderValues;
		if (Ar.IsLoading())
		{
			Content.Type = static_cast<EPlaceableType>(Type);
			Content.Facing = static_cast<EPlaceableDirection>(Facing);
		}
	}
}

void UGameSaveSubsystem::NotifyStepCompleted()
{
	if (PendingSlot.IsEmpty())
	{
		return;
	}
	const UGridSubsystem* G = ResolveGrid();
	const UEditSubsystem* E = ResolveEdit();
	if ((G && G->NumPendingPlacements() > 0) || (E && E->NumPendingEdits() > 0))
	{
		return; // intents queued during this step; they apply at N + 1
	}
	const FString Slot = PendingSlot;
	PendingSlot.Empty();
	SaveNow(Slot);
}

bool UGameSaveSubsystem::SaveNow(const FString& SlotName)
{
	UGridSubsystem* G = ResolveGrid();
	if (!G)
	{
		UE_LOG(LogAwsim, Warning, TEXT("Save: no grid subsystem."));
		return false;
	}
	const UEditSubsystem* E = ResolveEdit();
	if (G->NumPendingPlacements() > 0 || (E && E->NumPendingEdits() > 0))
	{
		UE_LOG(LogAwsim, Warning, TEXT("Save refused: intent queues are not empty (use RequestSave for the N + 1 flow)."));
		return false;
	}

	TArray<uint8> Bytes;
	FMemoryWriter Ar(Bytes);
	uint32 Magic = SaveMagic, Version = SaveVersion;
	Ar << Magic << Version;

	// Stats (missing subsystems save defaults; world-less specs hit this).
	const UWorld* World = GetWorld();
	const USimulationSubsystem* Sim = World ? World->GetSubsystem<USimulationSubsystem>() : nullptr;
	const UPopulationSubsystem* Population = World ? World->GetSubsystem<UPopulationSubsystem>() : nullptr;
	const UCityStatsSubsystem* Stats = World ? World->GetSubsystem<UCityStatsSubsystem>() : nullptr;
	const UGamePlayerFundsSubsystem* F = ResolveFunds();
	int32 Day = Sim ? Sim->GetDay() : 0;
	int32 StepCounter = Sim ? Sim->GetStepCounter() : 0;
	float GameSpeed = Sim ? Sim->GetGameSpeed() : 1.f;
	float Balance = F ? F->GetBalance() : 0.f;
	int32 PopulationCount = Population ? Population->GetCount() : 0;
	float Rating = Stats ? Stats->GetPlayerRating() : 0.5f;
	Ar << Day << StepCounter << GameSpeed << Balance << PopulationCount << Rating;

	// Def table: unique defs referenced by buildings and connectors.
	TArray<UPlaceableDef*> Defs;
	TMap<const UPlaceableDef*, int32> DefIndexOf;
	auto IndexOf = [&](const TObjectPtr<UPlaceableDef>& Def) -> int32
	{
		if (!Def) return INDEX_NONE;
		if (const int32* Found = DefIndexOf.Find(Def)) return *Found;
		const int32 Idx = Defs.Add(Def.Get());
		DefIndexOf.Add(Def, Idx);
		return Idx;
	};
	for (const FPlacedBuilding& B : G->GetBuildings()) IndexOf(B.Content.Definition);
	for (const TPair<FGridCoord, FGridContent>& R : G->GetRoads()) IndexOf(R.Value.Definition);
	for (const TPair<FGridCoord, FGridContent>& U : G->GetUtilities()) IndexOf(U.Value.Definition);

	int32 NumDefs = Defs.Num();
	Ar << NumDefs;
	for (UPlaceableDef* Def : Defs) SerializeDef(Ar, *Def);

	int32 NumBuildings = G->GetBuildings().Num();
	Ar << NumBuildings;
	for (const FPlacedBuilding& B : G->GetBuildings())
	{
		FPlacedBuilding Building = B; // serialization is non-const
		int32 DefIndex = IndexOf(B.Content.Definition);
		uint8 Lifetime = G->GetLifetime(B.Origin);
		Ar << Building.Origin.X << Building.Origin.Y << Building.HeightTiles << Lifetime;
		SerializeContent(Ar, Building.Content, DefIndex);
	}

	auto SaveConnectors = [&](const TMap<FGridCoord, FGridContent>& Map)
	{
		int32 Num = Map.Num();
		Ar << Num;
		for (const TPair<FGridCoord, FGridContent>& Pair : Map)
		{
			FGridCoord Tile = Pair.Key;
			FGridContent Content = Pair.Value;
			int32 DefIndex = IndexOf(Content.Definition);
			Ar << Tile.X << Tile.Y;
			SerializeContent(Ar, Content, DefIndex);
		}
	};
	SaveConnectors(G->GetRoads());
	SaveConnectors(G->GetUtilities());

	const FString Path = SlotToPath(SlotName);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), /*Tree*/ true);
	if (!FFileHelper::SaveArrayToFile(Bytes, *Path))
	{
		UE_LOG(LogAwsim, Warning, TEXT("Save: failed writing '%s'."), *Path);
		return false;
	}
	UE_LOG(LogAwsim, Log, TEXT("Saved '%s': %d building(s), %d road(s), %d utilit(ies), day %d."),
		*SlotName, NumBuildings, G->GetRoads().Num(), G->GetUtilities().Num(), Day);
	return true;
}

bool UGameSaveSubsystem::LoadNow(const FString& SlotName)
{
	UGridSubsystem* G = ResolveGrid();
	if (!G)
	{
		UE_LOG(LogAwsim, Warning, TEXT("Load: no grid subsystem."));
		return false;
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *SlotToPath(SlotName)))
	{
		UE_LOG(LogAwsim, Warning, TEXT("Load: cannot read '%s'."), *SlotToPath(SlotName));
		return false;
	}
	FMemoryReader Ar(Bytes);
	uint32 Magic = 0, Version = 0;
	Ar << Magic << Version;
	if (Magic != SaveMagic || Version != SaveVersion)
	{
		UE_LOG(LogAwsim, Warning, TEXT("Load: '%s' is not a compatible save (magic %08x, version %u)."), *SlotName, Magic, Version);
		return false;
	}

	int32 Day = 0, StepCounter = 0, PopulationCount = 0;
	float GameSpeed = 1.f, Balance = 0.f, Rating = 0.5f;
	Ar << Day << StepCounter << GameSpeed << Balance << PopulationCount << Rating;

	int32 NumDefs = 0;
	Ar << NumDefs;
	TArray<UPlaceableDef*> Defs;
	Defs.Reserve(NumDefs);
	for (int32 i = 0; i < NumDefs; ++i)
	{
		UPlaceableDef* Def = NewObject<UPlaceableDef>(GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UPlaceableDef::StaticClass(), TEXT("LoadedDef")));
		SerializeDef(Ar, *Def);
		Defs.Add(Def);
	}
	auto DefAt = [&](int32 Index) -> UPlaceableDef*
	{
		return Defs.IsValidIndex(Index) ? Defs[Index] : nullptr;
	};

	G->ResetForLoad();

	int32 NumBuildings = 0;
	Ar << NumBuildings;
	for (int32 i = 0; i < NumBuildings; ++i)
	{
		FPlacedBuilding Building;
		int32 DefIndex = INDEX_NONE;
		uint8 Lifetime = 0;
		Ar << Building.Origin.X << Building.Origin.Y << Building.HeightTiles << Lifetime;
		SerializeContent(Ar, Building.Content, DefIndex);
		Building.Content.Definition = DefAt(DefIndex);
		G->RestoreBuilding(Building, Lifetime);
	}

	auto LoadConnectors = [&]()
	{
		int32 Num = 0;
		Ar << Num;
		for (int32 i = 0; i < Num; ++i)
		{
			FGridCoord Tile;
			FGridContent Content;
			int32 DefIndex = INDEX_NONE;
			Ar << Tile.X << Tile.Y;
			SerializeContent(Ar, Content, DefIndex);
			Content.Definition = DefAt(DefIndex);
			G->RestoreConnector(Tile, Content);
		}
	};
	LoadConnectors(); // roads
	LoadConnectors(); // utilities

	if (UGamePlayerFundsSubsystem* F = ResolveFunds())
	{
		F->RestoreBalance(Balance);
	}
	UWorld* World = GetWorld();
	if (USimulationSubsystem* Sim = World ? World->GetSubsystem<USimulationSubsystem>() : nullptr)
	{
		Sim->RestoreClock(Day, StepCounter);
		Sim->SetGameSpeed(GameSpeed);
	}
	if (UPopulationSubsystem* Population = World ? World->GetSubsystem<UPopulationSubsystem>() : nullptr)
	{
		Population->SetCount(PopulationCount);
	}
	if (UCityStatsSubsystem* Stats = World ? World->GetSubsystem<UCityStatsSubsystem>() : nullptr)
	{
		Stats->RestorePlayerRating(Rating);
	}

	UE_LOG(LogAwsim, Log, TEXT("Loaded '%s': %d building(s), day %d, balance %.0f."), *SlotName, NumBuildings, Day, Balance);
	return true;
}

FString UGameSaveSubsystem::SlotToPath(const FString& SlotName)
{
	return FPaths::ProjectSavedDir() / TEXT("SaveGames") / SlotName + TEXT(".awsim");
}

UGridSubsystem* UGameSaveSubsystem::ResolveGrid() const
{
	if (Grid) return Grid;
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UGridSubsystem>() : nullptr;
}

UEditSubsystem* UGameSaveSubsystem::ResolveEdit() const
{
	if (Edit) return Edit;
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UEditSubsystem>() : nullptr;
}

UGamePlayerFundsSubsystem* UGameSaveSubsystem::ResolveFunds() const
{
	if (Funds) return Funds;
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UGamePlayerFundsSubsystem>() : nullptr;
}
