#include "Misc/AutomationTest.h"

// BEHAVIOUR SPEC — written BEFORE implementation (BDD-first). It() names are the
// contract; bodies are PENDING stubs sketching the intended API in comments.
// Water mirrors Energy: global capacity, island-scoped service.

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FWaterSpec, "awsim.Simulation.Water",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)
END_DEFINE_SPEC(FWaterSpec)

void FWaterSpec::Define()
{
	Describe("Starting state", [this]()
	{
		It("starts with water capacity 0", [this]()
		{
			// expect: Water->GetCapacity() == 0
		});
	});

	Describe("Capacity — counts regardless of connectivity", [this]()
	{
		It("raises total capacity for any water building, even one alone on its own island", [this]() {});

		It("changes total capacity by the building's signed water contribution", [this]() {});
	});

	Describe("Service is island-scoped — a water plant only serves its own island", [this]()
	{
		It("meets demand for consumers in the SAME island when local supply >= local demand", [this]() {});

		It("does NOT supply consumers in a DIFFERENT island from the plant", [this]() {});

		It("leaves an island under-supplied even when another island has surplus", [this]() {});
	});
}

#endif // WITH_AUTOMATION_TESTS
