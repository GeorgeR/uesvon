#pragma once

#include "Async/AsyncWork.h"
#include "SVONLink.h"
#include "SVONTypes.h"
#include "SVONPathFinder.h"
#include "ThreadSafeBool.h"

class ASVONVolumeActor;
struct FSVONPathFinderSettings;

class FSVONFindPathTask 
    : public FNonAbandonableTask
{
	friend class FAutoDeleteAsyncTask<FSVONFindPathTask>;

public:
	FSVONFindPathTask(ASVONVolumeActor& Volume, FSVONPathFinderSettings& Settings, UWorld* World,
		const FSVONLink Start, const FSVONLink Target,
		const FVector& StartLocation, const FVector& TargetLocation,
		FSVONNavPathSharedPtr* Path, FThreadSafeBool& CompleteFlag, TArray<FVector>& DebugOpenPoints)
		: Volume(Volume),
		Settings(Settings),
			World(World),
			Start(Start),
			Target(Target),
			StartLocation(StartLocation),
			TargetLocation(TargetLocation),
			Path(Path),
			CompleteFlag(CompleteFlag),
			DebugOpenPoints(DebugOpenPoints) { }

protected:
	ASVONVolumeActor& Volume;
	FSVONPathFinderSettings Settings;
	UWorld* World;

	FSVONLink Start;
	FSVONLink Target;
	FVector StartLocation;
	FVector TargetLocation;
	FSVONNavPathSharedPtr* Path;

	FThreadSafeBool& CompleteFlag;
	TArray<FVector>& DebugOpenPoints;

	void DoWork();

	// This next section of Code needs to be here.  Not important as to why.
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FSVONFindPathTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};
