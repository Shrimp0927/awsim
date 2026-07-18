#include "Camera/GameCameraPawn.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "GameFramework/PlayerController.h"

AGameCameraPawn::AGameCameraPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetRootComponent());
}

void AGameCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	Camera->SetFieldOfView(Fov);
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true; // for the future picking pass
	}
	Target = FVector2D::ZeroVector; // the grid is centered on the world origin
	UpdateCamera();
}

// Code-first project: actions and mappings are built at runtime, not as assets.
void AGameCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}

	PanAction = NewObject<UInputAction>(this);
	PanAction->ValueType = EInputActionValueType::Axis2D;
	ZoomAction = NewObject<UInputAction>(this);
	ZoomAction->ValueType = EInputActionValueType::Axis1D;
	RotateAction = NewObject<UInputAction>(this);
	RotateAction->ValueType = EInputActionValueType::Boolean;

	UInputMappingContext* Context = NewObject<UInputMappingContext>(this);
	auto MapKey = [&](UInputAction* Action, const FKey& Key, bool bNegate = false, bool bToY = false)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (bToY)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this)); // default YXZ: key value lands on action Y
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		}
	};
	// Pan action: X = screen right, Y = screen up.
	MapKey(PanAction, EKeys::D);
	MapKey(PanAction, EKeys::A, /*bNegate*/ true);
	MapKey(PanAction, EKeys::W, false, /*bToY*/ true);
	MapKey(PanAction, EKeys::S, true, true);
	MapKey(ZoomAction, EKeys::MouseWheelAxis);
	MapKey(RotateAction, EKeys::R);

	Input->BindAction(PanAction, ETriggerEvent::Triggered, this, &AGameCameraPawn::OnPan);
	Input->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AGameCameraPawn::OnZoom);
	Input->BindAction(RotateAction, ETriggerEvent::Started, this, &AGameCameraPawn::OnRotate); // once per press, not while held

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(Context, 0);
		}
	}
}

void AGameCameraPawn::OnPan(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	// Ground-plane screen axes: "up" is the camera's forward projected onto the grid.
	const float YawRad = FMath::DegreesToRadians(CurrentYaw());
	const FVector2D Up(FMath::Cos(YawRad), FMath::Sin(YawRad));
	const FVector2D Right(-Up.Y, Up.X);
	const float Step = PanSpeed * Distance * GetWorld()->GetDeltaSeconds();
	Target += (Right * Axis.X + Up * Axis.Y) * Step;
	UpdateCamera();
}

void AGameCameraPawn::OnZoom(const FInputActionValue& Value)
{
	Distance *= FMath::Pow(ZoomStep, -Value.Get<float>()); // wheel up zooms in
	UpdateCamera();
}

void AGameCameraPawn::OnRotate(const FInputActionValue& Value)
{
	YawSteps = (YawSteps + 1) % 4;
	UpdateCamera();
}

void AGameCameraPawn::UpdateCamera()
{
	const float MaxX = UGridSubsystem::WorldMinX + UGridSubsystem::GetWidth() * UGridSubsystem::TileSize;
	const float MaxY = UGridSubsystem::WorldMinY + UGridSubsystem::GetHeight() * UGridSubsystem::TileSize;
	Target.X = FMath::Clamp(Target.X, UGridSubsystem::WorldMinX + GridPadding, MaxX - GridPadding);
	Target.Y = FMath::Clamp(Target.Y, UGridSubsystem::WorldMinY + GridPadding, MaxY - GridPadding);
	Distance = FMath::Clamp(Distance, MinDistance, MaxDistance);

	const FRotator View(PitchDegrees, CurrentYaw(), 0.f);
	SetActorLocationAndRotation(FVector(Target, 0.f) - View.Vector() * Distance, View);
}
