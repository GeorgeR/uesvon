#pragma once

#include "CoreMinimal.h"

#include "SVONTypes.h"
#include "SVONNavigationPath.h"
#include "SVONLink.h"

struct FSVONNavigationPath;
class ASVONVolumeActor;

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
		: bDebugOpenNodes(false),
		bUseUnitCost(false),
		UnitCost(1.0f),
		WeightEstimate(1.0f),
		NodeSizeCompensation(1.0f),
		SmoothingIterations(0.f),
		PathCostType(ESVONPathCostType::SPCT_Euclidean) {}
};

class UESVON_API FSVONPathFinder
{
public:
	FSVONPathFinder(UWorld* World, const ASVONVolumeActor& Volume, FSVONPathFinderSettings& Settings)
		: World(World),
		Volume(Volume),
		Settings(Settings) { };

	~FSVONPathFinder() { };

	/* Performs an A* search from start to target navlink */
	int32 FindPath(const FSVONLink& Start, const FSVONLink& Target, const FVector& StartLocation, const FVector& TargetLocation, FSVONNavPathSharedPtr* OutPath);

	//FORCEINLINE const FSVONNavigationPath& GetPath() const { return Path; }
	//const FNavigationPath& GetNavPath();  

private:
	TArray<FSVONLink> OpenSet;
	TSet<FSVONLink> ClosedSet;

	TMap<FSVONLink, FSVONLink> CameFrom;

	TMap<FSVONLink, float> GScore;
	TMap<FSVONLink, float> FScore;

	FSVONLink Start;
	FSVONLink Current;
	FSVONLink Goal;

	UWorld* World;
	const ASVONVolumeActor& Volume;
	FSVONPathFinderSettings& Settings;

	/* A* heuristic calculation */
	float HeuristicScore(const FSVONLink& Start, const FSVONLink& Target);

	/* Distance between two links */
	float GetCost(const FSVONLink& Start, const FSVONLink& Target);

	void ProcessLink(const FSVONLink& Neighbor);

	/* Constructs the path by navigating back through our CameFrom map */
	void BuildPath(const TMap<FSVONLink, FSVONLink>& InCameFrom, FSVONLink Current, const FVector& StartLocation, const FVector& TargetLocation, FSVONNavPathSharedPtr* OutPath);

	/*void Smooth_Chaikin(TArray<FVector>& somePoints, int aNumIterations);*/
};
