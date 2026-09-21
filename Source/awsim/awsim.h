// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

AWSIM_API DECLARE_LOG_CATEGORY_EXTERN(LogAwsim, Log, All);


CSV_DECLARE_CATEGORY_MODULE_EXTERN(AWSIM_API, Awsim);

// One scope feeds both Unreal Insights (Awsim_<Name>) and the CSV profiler (Awsim/<Name>).
#define AWSIM_PERF_SCOPE(Name) \
	TRACE_CPUPROFILER_EVENT_SCOPE(Awsim_##Name); \
	CSV_SCOPED_TIMING_STAT(Awsim, Name)
