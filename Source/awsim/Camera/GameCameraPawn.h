#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameCameraPawn.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

// God-view camera with yaw fixed at -90 (facing -Y): screen right = +X, screen up = -Y, so grid (0,0) is the map's top-left.
// The frustum's ground footprint is clamped to never leave the grid.
UCLASS()
class AWSIM_API AGameCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AGameCameraPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void OnZoom(const FInputActionValue& Value);
	void EdgePan(float DeltaSeconds);
	float ViewportAspect() const;
	void UpdateZoomBounds();
	void ClampFocusToGrid();
	void ApplyView();

	// Screen->world mapping assumes this yaw; not a tuning knob.
	static constexpr float YawDegrees = -90.f;

	UPROPERTY(EditAnywhere, Category="Camera") float PitchDegrees = -75.f;
	UPROPERTY(EditAnywhere, Category="Camera") float Fov = 50.f;                // horizontal, fixed — zoom is distance only
	UPROPERTY(EditAnywhere, Category="Camera") float MinZoom = 2000.f;          // closest distance to the focus point
	UPROPERTY(EditAnywhere, Category="Camera") float ZoomNotchFraction = 0.15f; // one wheel notch scales zoom by this much
	UPROPERTY(EditAnywhere, Category="Camera") float ZoomInterpSpeed = 8.f;
	UPROPERTY(EditAnywhere, Category="Camera") float EdgeMarginPx = 25.f;
	UPROPERTY(EditAnywhere, Category="Camera") float EdgePanSpeed = 1.2f;       // fraction of zoom distance per second

	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;

	// Built at runtime; authored InputAction assets can replace these later.
	UPROPERTY(Transient) TObjectPtr<UInputAction> ZoomAction;
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> InputContext;

	FVector Focus = FVector::ZeroVector; // ground point the camera orbits above
	// Derived each tick from FOV/aspect: the largest zoom whose ground footprint still fits inside the grid.
	float MaxZoom = 40000.f;
	float TargetZoom = 0.f;
	float CurrentZoom = 0.f;
	FVector2D GridWorldSize = FVector2D(100000.f, 100000.f);
};
