#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UEconomySubsystem;
class UEnergySubsystem;
class UGamePlayerFundsSubsystem;
class UGameWaterSubsystem;
class UPopulationSubsystem;
class USimulationSubsystem;

// Bottom stats bar during play; read-only, never blocks world clicks.
class SAwsimHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAwsimHud) {}
		SLATE_ARGUMENT(TWeakObjectPtr<USimulationSubsystem>, Sim)
		SLATE_ARGUMENT(TWeakObjectPtr<UGamePlayerFundsSubsystem>, Funds)
		SLATE_ARGUMENT(TWeakObjectPtr<UPopulationSubsystem>, Population)
		SLATE_ARGUMENT(TWeakObjectPtr<UEnergySubsystem>, Energy)
		SLATE_ARGUMENT(TWeakObjectPtr<UGameWaterSubsystem>, Water)
		SLATE_ARGUMENT(TWeakObjectPtr<UEconomySubsystem>, Economy)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TWeakObjectPtr<USimulationSubsystem> Sim;
	TWeakObjectPtr<UGamePlayerFundsSubsystem> Funds;
	TWeakObjectPtr<UPopulationSubsystem> Population;
	TWeakObjectPtr<UEnergySubsystem> Energy;
	TWeakObjectPtr<UGameWaterSubsystem> Water;
	TWeakObjectPtr<UEconomySubsystem> Economy;
};
