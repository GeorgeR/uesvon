#include "SVONFindPathTask.h"

#include "SVONPathFinder.h"

void FSVONFindPathTask::DoWork()
{
	FSVONPathFinder PathFinder(World, Volume, Settings);
	auto Result = PathFinder.FindPath(Start, Target, StartLocation, TargetLocation, Path);
	CompleteFlag = true;
}
