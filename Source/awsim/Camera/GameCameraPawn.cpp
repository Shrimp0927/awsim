#include "Camera/GameCameraPawn.h"
#include "Simulation/GameGridSubsystem.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

AGameCameraPawn::AGameCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetRootComponent());
}

void AGameCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	GridWorldSize = FVector2D(
		UGridSubsystem::GetWidth() * UGridSubsystem::TileSize,
		UGridSubsystem::GetHeight() * UGridSubsystem::TileSize);

	if (Camera)
	{
		Camera->SetFieldOfView(Fov);
	}

	UpdateZoomBounds();
	Focus = FVector(GridWorldSize.X * 0.5f, GridWorldSize.Y * 0.5f, 0.f);
	TargetZoom = MaxZoom;
	CurrentZoom = MaxZoom;
	ApplyView();

	// Edge panning steers by cursor position, so the cursor must be visible.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
	}
}

void AGameCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	ZoomAction = NewObject<UInputAction>(this);
	ZoomAction->ValueType = EInputActionValueType::Axis1D;
	InputContext = NewObject<UInputMappingContext>(this);
	InputContext->MapKey(ZoomAction, EKeys::MouseWheelAxis);

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Input->AddMappingContext(InputContext, 0);
		}
	}
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		Input->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AGameCameraPawn::OnZoom);
	}
}

void AGameCameraPawn::OnZoom(const FInputActionValue& Value)
{
	// Exponential zoom: each notch scales the target, so perceived speed is the same at any height.
	const float Notches = Value.Get<float>();
	TargetZoom = FMath::Clamp(
		TargetZoom * FMath::Pow(1.f - ZoomNotchFraction, Notches), MinZoom, MaxZoom);
}

void AGameCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateZoomBounds(); // aspect can change with the window
	TargetZoom = FMath::Clamp(TargetZoom, MinZoom, MaxZoom);
	CurrentZoom = FMath::Clamp(
		FMath::FInterpTo(CurrentZoom, TargetZoom, DeltaSeconds, ZoomInterpSpeed), MinZoom, MaxZoom);
	EdgePan(DeltaSeconds);
	ApplyView();
}

void AGameCameraPawn::EdgePan(float DeltaSeconds)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	int32 ViewX = 0, ViewY = 0;
	PC->GetViewportSize(ViewX, ViewY);
	float MouseX = 0.f, MouseY = 0.f;
	if (ViewX <= 0 || ViewY <= 0 || !PC->GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	FVector2D ScreenDir(0.f, 0.f); // X: right, Y: up
	if (MouseX <= EdgeMarginPx)              ScreenDir.X = -1.f;
	else if (MouseX >= ViewX - EdgeMarginPx) ScreenDir.X = 1.f;
	if (MouseY <= EdgeMarginPx)              ScreenDir.Y = 1.f;
	else if (MouseY >= ViewY - EdgeMarginPx) ScreenDir.Y = -1.f;
	if (ScreenDir.IsZero())
	{
		return;
	}

	// Yaw is fixed at -90: screen right = world +X, screen up = world -Y.
	// Pan scales with zoom so a screen-width sweep feels the same at any height.
	const float Speed = CurrentZoom * EdgePanSpeed * DeltaSeconds;
	Focus.X += ScreenDir.X * Speed;
	Focus.Y -= ScreenDir.Y * Speed;
}

namespace
{
	// Frustum-ground geometry per unit of zoom: NearHit/FarHit are where the bottom/top screen edges hit
	// the ground, HalfWidth is half the widest visible ground row. Requires pitch > half-FOV (no horizon on screen).
	struct FGroundFootprint
	{
		float Back = 0.f;
		float NearHit = 0.f;
		float FarHit = 0.f;
		float HalfWidth = 0.f;
		bool bValid = false;
	};

	FGroundFootprint FootprintAt(float Zoom, float PitchDegrees, float FovDegrees, float Aspect)
	{
		FGroundFootprint Out;
		const float Pitch = FMath::DegreesToRadians(-PitchDegrees); // downward, positive
		// UCameraComponent::FieldOfView is the HORIZONTAL angle; the vertical one comes from the aspect.
		const float HalfH = FMath::DegreesToRadians(FovDegrees * 0.5f);
		const float HalfV = FMath::Atan(FMath::Tan(HalfH) / FMath::Max(Aspect, 0.1f));
		const float FarAngle = Pitch - HalfV;
		if (FarAngle <= KINDA_SMALL_NUMBER)
		{
			return Out; // horizon on screen — footprint unbounded
		}

		const float Height = Zoom * FMath::Sin(Pitch);
		Out.Back = Zoom * FMath::Cos(Pitch); // camera sits this far behind the focus
		Out.NearHit = Height / FMath::Tan(Pitch + HalfV);
		Out.FarHit = Height / FMath::Tan(FarAngle);
		Out.HalfWidth = (Height / FMath::Sin(FarAngle)) * FMath::Tan(HalfH);
		Out.bValid = true;
		return Out;
	}
}

float AGameCameraPawn::ViewportAspect() const
{
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		int32 ViewX = 0, ViewY = 0;
		PC->GetViewportSize(ViewX, ViewY);
		if (ViewX > 0 && ViewY > 0)
		{
			return static_cast<float>(ViewX) / static_cast<float>(ViewY);
		}
	}
	return 16.f / 9.f;
}

void AGameCameraPawn::UpdateZoomBounds()
{
	// The footprint scales linearly with zoom, so measure it at zoom 1 and divide.
	const FGroundFootprint Unit = FootprintAt(1.f, PitchDegrees, Fov, ViewportAspect());
	if (!Unit.bValid)
	{
		return; // degenerate pitch/FOV combination — keep the previous bound
	}

	// Facing -Y: frustum depth spans Y, width spans X.
	const float DepthPerZoom = Unit.FarHit - Unit.NearHit;
	const float WidthPerZoom = 2.f * Unit.HalfWidth;
	MaxZoom = FMath::Min(
		GridWorldSize.Y / FMath::Max(DepthPerZoom, KINDA_SMALL_NUMBER),
		GridWorldSize.X / FMath::Max(WidthPerZoom, KINDA_SMALL_NUMBER));
	MaxZoom = FMath::Max(MaxZoom, MinZoom);
}

void AGameCameraPawn::ClampFocusToGrid()
{
	// Clamp the frustum's ground footprint to the grid, not merely the focus point — the tilted camera sees
	// well past the focus. MaxZoom caps the footprint to the grid, so the per-axis bounds never invert.
	const FGroundFootprint Foot = FootprintAt(CurrentZoom, PitchDegrees, Fov, ViewportAspect());
	if (!Foot.bValid)
	{
		return;
	}

	// Facing -Y: the camera sits Back beyond the focus in +Y, the far screen edge reaches toward -Y.
	const float MinX = Foot.HalfWidth;
	const float MaxX = GridWorldSize.X - Foot.HalfWidth;
	const float MinY = Foot.FarHit - Foot.Back;
	const float MaxY = GridWorldSize.Y + Foot.NearHit - Foot.Back;

	Focus.X = MinX <= MaxX ? FMath::Clamp(Focus.X, MinX, MaxX) : GridWorldSize.X * 0.5f;
	Focus.Y = MinY <= MaxY ? FMath::Clamp(Focus.Y, MinY, MaxY) : GridWorldSize.Y * 0.5f;
}

void AGameCameraPawn::ApplyView()
{
	ClampFocusToGrid();

	const FRotator View(PitchDegrees, YawDegrees, 0.f);
	SetActorLocationAndRotation(Focus - View.Vector() * CurrentZoom, View);
}
