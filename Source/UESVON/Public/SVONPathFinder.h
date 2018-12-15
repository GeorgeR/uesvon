#pragma once

#include "CoreMinimal.h"
#include "SVONPath.h"
#include <NavigationData.h>
#include "SVONLink.h"

struct FSVONPath;
class ASVONVolume;

struct FSVONPathFinderSettings
{
	bool bDebugOpenNodes;
	bool bUseUnitCost;
	float UnitCost;
	float WeightEstimate;
	float NodeSizeCompensation;
	int SmoothingIterations;
	ESVONPathCostType PathCostType;
	TArray<FVector> DebugPoints;

	FSVONPathFinderSettings()
		: bDebugOpenNodes(false)
		, bUseUnitCost(false)
		, UnitCost(1.0f)
		, WeightEstimate(1.0f)
		, NodeSizeCompensation(1.0f)
		, SmoothingIterations(0.f)
		, PathCostType(ESVONPathCostType::SPCT_Euclidean) {}
};

class UESVON_API FSVONPathFinder
{
public:
	FSVONPathFinder(UWorld* World, const ASVONVolume& Volume, FSVONPathFinderSettings& Settings)
		: World(World),
		Volume(Volume),
		Settings(Settings) { };

	~FSVONPathFinder() { };

	/* Performs an A* search from start to target navlink */
	int FindPath(const FSVONLink& Start, const FSVONLink& Target, const FVector& StartLocation, const FVector& TargetLocation, FNavPathSharedPtr* OutPath);

	FORCEINLINE const FSVONPath& GetPath() const { return Path; }
	//const FNavigationPath& GetNavPath();  

private:
	FSVONPath Path;

	FNavigationPath NavPath;

	TArray<FSVONLink> OpenSet;
	TSet<FSVONLink> ClosedSet;

	TMap<FSVONLink, FSVONLink> CameFrom;

	TMap<FSVONLink, float> GScore;
	TMap<FSVONLink, float> FScore;

	FSVONLink Current;
	FSVONLink Goal;

	const ASVONVolume& Volume;

	FSVONPathFinderSettings& Settings;

	UWorld* World;

	/* A* heuristic calculation */
	float HeuristicScore(const FSVONLink& Start, const FSVONLink& Target);

	/* Distance between two links */
	float GetCost(const FSVONLink& Start, const FSVONLink& Target);

	void ProcessLink(const FSVONLink& Neighbor);

	/* Constructs the path by navigating back through our CameFrom map */
	void BuildPath(TMap<FSVONLink, FSVONLink>& CameFrom, FSVONLink Current, const FVector& StartLocation, const FVector& TargetLocation, FNavPathSharedPtr* OutPath);

	void Smooth_Chaikin(TArray<FVector>& somePoints, int aNumIterations);
};