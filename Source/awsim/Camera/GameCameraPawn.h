#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Simulation/GameGridSubsystem.h"
#include "GameCameraPawn.generated.h"

class UCameraComponent;
class UInputAction;
struct FInputActionValue;

// God-view camera: state is a look-at target T (clamped to the grid) and a
// boom distance d; the position is always derived as P = T - ViewDir * d.
UCLASS()
class AWSIM_API AGameCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AGameCameraPawn();

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void OnPan(const FInputActionValue& Value);
	void OnZoom(const FInputActionValue& Value);
	void OnRotate(const FInputActionValue& Value);
	void UpdateCamera();

	// R cycles the ground boom S -> E -> N -> W; each step is -90 degrees of view yaw.
	float CurrentYaw() const { return -90.f - 90.f * YawSteps; }

	// -60: steeper pitches foreshorten walls until buildings are unreadable.
	UPROPERTY(EditAnywhere, Category="Camera") float PitchDegrees = -60.f;
	UPROPERTY(EditAnywhere, Category="Camera") float Fov = 50.f; // horizontal
	UPROPERTY(EditAnywhere, Category="Camera") float MinDistance = 3000.f;
	UPROPERTY(EditAnywhere, Category="Camera") float MaxDistance = 70000.f;
	// Scaled by Distance so panning covers a constant fraction of the screen.
	UPROPERTY(EditAnywhere, Category="Camera") float PanSpeed = 1.f;
	UPROPERTY(EditAnywhere, Category="Camera") float ZoomStep = 1.15f; // Distance multiplier per wheel tick

	FVector2D Target = FVector2D::ZeroVector; // T: look-at point on the grid plane
	float Distance = 40000.f;                 // d: boom length from T to the camera
	int32 YawSteps = 0;                       // 0..3 quarter turns from the default orientation
	static constexpr float GridPadding = UGridSubsystem::TileSize * 20.f;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY() TObjectPtr<UInputAction> PanAction;
	UPROPERTY() TObjectPtr<UInputAction> ZoomAction;
	UPROPERTY() TObjectPtr<UInputAction> RotateAction;
};
