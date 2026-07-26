#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GameUiSubsystem.generated.h"

// Owns the Slate UI roots: menu on the viewport while the flow state is Menu,
// the stats HUD while Playing.
UCLASS()
class AWSIM_API UGameUiSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

private:
	void RemoveWidget(TSharedPtr<SWidget>& Widget);

	TSharedPtr<SWidget> MenuWidget;
	TSharedPtr<SWidget> HudWidget;
};
