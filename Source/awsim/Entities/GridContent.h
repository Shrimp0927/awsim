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
enum class EDirection : uint8
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
	UPROPERTY(EditAnywhere) float Value = 0.5f;
	UPROPERTY(EditAnywhere) TArray<FDomainEffect> Effects;
};

UCLASS()
class UPlaceableDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere) EPlaceableType Type = EPlaceableType::None;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> Mesh;
	UPROPERTY(EditAnywhere) TArray<FSliderDef> Sliders;
	UPROPERTY(EditAnywhere) FIntPoint Dimensions = {1, 1}; // The width x len when facing North
};

USTRUCT()
struct FGridContent
{
	GENERATED_BODY()

	UPROPERTY() EPlaceableType Type = EPlaceableType::None;
	UPROPERTY() EDirection Facing = EDirection::None;
	UPROPERTY() TObjectPtr<UPlaceableDef> Definition;
};
