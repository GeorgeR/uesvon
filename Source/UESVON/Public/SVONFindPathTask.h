#pragma once

#include "Runtime/Core/Public/Async/AsyncWork.h"

class FSVONFindPathTask 
    : public FNonAbandonableTask
{
	friend class FAutoDeleteAsyncTask<FSVONFindPathTask>;

public:
	FSVONFindPathTask(ASVONVolume& aVolume, UWorld* aWorld, const FSVONLink aStart, const FSVONLink aTarget, const FVector& aStartPos, const FVector& aTargetPos, FNavPathSharedPtr* oPath, TQueue<int>& aQueue, TArray<FVector>& aDebugOpenPoints) :
		Volume(aVolume),
		World(aWorld),
		myStart(aStart),
		myTarget(aTarget),
		myStartPos(aStartPos),
		myTargetPos(aTargetPos),
		Path(oPath),
		myOutQueue(aQueue),
		myDebugOpenPoints(aDebugOpenPoints)
	{}

protected:
	ASVONVolume& Volume;
	UWorld* World;

	FSVONLink myStart;
	FSVONLink myTarget;
	FVector myStartPos;
	FVector myTargetPos;
	FNavPathSharedPtr* Path;

	TQueue<int>& myOutQueue;
	TArray<FVector>& myDebugOpenPoints;

	void DoWork()
	{
		FSVONPathFinderSettings settings;

		FSVONPathFinder pathFinder(World, Volume, settings);

		int result = pathFinder.FindPath(myStart, myTarget, myStartPos, myTargetPos, Path);

		myOutQueue.Enqueue(result);
		
	}

	// This next section of Code needs to be here.  Not important as to why.

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FSVONFindPathTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};