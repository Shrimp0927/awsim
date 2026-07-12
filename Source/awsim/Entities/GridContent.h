#pragma once

#include "CoreMinimal.h"
#include "Math/Interval.h"
#include "Math/MathFwd.h"

#include "GridContent.generated.h"

UENUM()
enum class EPlaceableType : uint8
{
	None,
	Building,
	Road,
	Utility,
	Environment
};

UENUM()
enum class EPlaceableDirection : uint8
{
	None,
	North,
	East,
	South,
	West
};

UENUM()
enum class EDomain : uint8
{
	None,
	Housing,
	Economy,
	Energy,
	Water
};

USTRUCT()
struct FDomainEffect
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) EDomain Domain = EDomain::None;
	UPROPERTY(EditAnywhere) float AmountAtMin = 0.f; // signed value
	UPROPERTY(EditAnywhere) float AmountAtMax = 0.f; // signed value
};

USTRUCT()
struct FSliderDef
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName Name;
	UPROPERTY(EditAnywhere) FFloatInterval Range = {0.f, 1.f};
	UPROPERTY(EditAnywhere) float Value = 0.5f; // authoring default; the live value is per-instance (FGridContent::SliderValues)
	UPROPERTY(EditAnywhere) TArray<FDomainEffect> Effects;
};

UCLASS()
class UPlaceableDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere) EPlaceableType Type = EPlaceableType::None;
	UPROPERTY(EditAnywhere) float Cost = 0.f; // placement price, paid from player funds
	UPROPERTY(EditAnywhere) float DailyMaintenanceCost = 0.f; // daily upkeep cost this gets spend override=true
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> Mesh;
	UPROPERTY(EditAnywhere) TArray<FSliderDef> Sliders;
	UPROPERTY(EditAnywhere) FIntPoint Dimensions = {1, 1}; // The width x len when facing North
	UPROPERTY(EditAnywhere) EDomain ConnectorDomain = EDomain::None; // For Utility connectors: Energy = power line, Water = pipe
};

USTRUCT()
struct FGridContent
{
	GENERATED_BODY()

	UPROPERTY() EPlaceableType Type = EPlaceableType::None;
	UPROPERTY() EPlaceableDirection Facing = EPlaceableDirection::None;
	UPROPERTY() TObjectPtr<UPlaceableDef> Definition;

	// Per-instance slider values, parallel to Definition->Sliders. Seeded from
	// the def's authored defaults on placement; domain subsystems read these,
	// never the def's Value.
	UPROPERTY() TArray<float> SliderValues;
};

// Signed contribution of this content's sliders for a domain, interpolated by
// each slider's live per-instance value across its range (the def's Value is
// only the authoring default). Shared by every domain phase.
inline float DomainAmount(const FGridContent& Content, EDomain InDomain)
{
	if (!Content.Definition) return 0.f;
	float Total = 0.f;
	const TArray<FSliderDef>& Sliders = Content.Definition->Sliders;
	for (int32 i = 0; i < Sliders.Num(); ++i)
	{
		const FSliderDef& Slider = Sliders[i];
		const float Value = Content.SliderValues.IsValidIndex(i) ? Content.SliderValues[i] : Slider.Value;
		const float Span = Slider.Range.Max - Slider.Range.Min;
		const float Alpha = Span > 0.f ? (Value - Slider.Range.Min) / Span : 0.f;
		for (const FDomainEffect& Effect : Slider.Effects)
		{
			if (Effect.Domain == InDomain)
			{
				Total += FMath::Lerp(Effect.AmountAtMin, Effect.AmountAtMax, Alpha);
			}
		}
	}
	return Total;
}

// Whether a def touches a domain at all (any slider effect, either sign).
inline bool DefHasDomain(const UPlaceableDef* Def, EDomain InDomain)
{
	if (!Def) return false;
	for (const FSliderDef& Slider : Def->Sliders)
	{
		for (const FDomainEffect& Effect : Slider.Effects)
		{
			if (Effect.Domain == InDomain) return true;
		}
	}
	return false;
}
