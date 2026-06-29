#include "Misc/AutomationTest.h"

// BEHAVIOUR SPEC — written BEFORE implementation (BDD-first). It() names are the
// contract; bodies are PENDING stubs sketching the intended API in comments.

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FHousingSpec, "awsim.Simulation.Housing",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)
END_DEFINE_SPEC(FHousingSpec)

void FHousingSpec::Define()
{
	Describe("Starting state", [this]()
	{
		It("starts with housing capacity 0", [this]()
		{
			// expect: Housing->GetCapacity() == 0   (ASSUMPTION: starts at 0 like energy/water)
		});
	});

	Describe("Building contributions", [this]()
	{
		It("raises housing capacity for a building with positive housing", [this]() {});

		It("lowers housing capacity for a building with negative housing", [this]() {});
	});

	Describe("Island servicing — OPEN QUESTION", [this]()
	{
		It("[TBD] whether a home only counts when its island has energy + water", [this]()
		{
			// Energy/Water service is island-scoped. Undecided whether housing (and
			// economy) only "function" when their island is actually serviced, or
			// whether their capacity counts regardless. Confirm before implementing.
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
