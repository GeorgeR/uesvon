#include "SVONPathFinder.h"

#include "SVONLink.h"
#include "NavigationData.h"

int32 FSVONPathFinder::FindPath(const FSVONLink& Start, const FSVONLink& Goal, const FVector& StartLocation, const FVector& TargetLocation, FNavPathSharedPtr* OutPath)
{
	OpenSet.Empty();
	ClosedSet.Empty();
	CameFrom.Empty();
	FScore.Empty();
	GScore.Empty();
	Current = FSVONLink();
	this->Goal = Goal;

    OpenSet.Add(Start);
	CameFrom.Add(Start, Start);
	GScore.Add(Start, 0);
	FScore.Add(Start, HeuristicScore(Start, this->Goal)); // Distance to target

	int NumIterations = 0;
	while (OpenSet.Num() > 0)
	{
		float LowestScore = FLT_MAX;
		for (FSVONLink& Link : OpenSet)
		{
			if (!FScore.Contains(Link) || FScore[Link] < LowestScore)
			{
				LowestScore = FScore[Link];
				Current = Link;
			}
		}

		OpenSet.Remove(Current);
		ClosedSet.Add(Current);

		if (Current == Goal)
		{
			BuildPath(CameFrom, Current, StartLocation, TargetLocation, OutPath);
			UE_LOG(UESVON, Display, TEXT("Pathfinding complete, iterations : %i"), NumIterations);
			return 1;
		}

		const FSVONNode& CurrentNode = Volume.GetNode(Current);

		TArray<FSVONLink> Neighbors;
		if (Current.LayerIndex == 0 && CurrentNode.FirstChild.IsValid())
			Volume.GetLeafNeighbors(Current, Neighbors);
		else
			Volume.GetNeighbors(Current, Neighbors);

		for (const FSVONLink& Neighbor : Neighbors)
			ProcessLink(Neighbor);

		NumIterations++;
	}

	UE_LOG(UESVON, Display, TEXT("Pathfinding failed, iterations : %i"), NumIterations);

	return 0;
}

float FSVONPathFinder::HeuristicScore(const FSVONLink& Start, const FSVONLink& Target)
{
	/* Just using manhattan distance for now */
	float Score = 0.f;

	FVector StartLocation, EndLocation;
	Volume.GetLinkLocation(Start, StartLocation);
	Volume.GetLinkLocation(Target, EndLocation);
	switch (Settings.PathCostType)
	{
		case ESVONPathCostType::SPCT_Manhattan:
			Score = FMath::Abs(EndLocation.X - StartLocation.X) + FMath::Abs(EndLocation.Y - StartLocation.Y) + FMath::Abs(EndLocation.Z - StartLocation.Z);
			break;

		case ESVONPathCostType::SPCT_Euclidean:
		default:
			Score = (StartLocation - EndLocation).Size();
			break;
	}
	
	Score *= (1.0f - (static_cast<float>(Target.LayerIndex) / static_cast<float>(Volume.GetNumLayers())) * Settings.NodeSizeCompensation);

	return Score;
}

float FSVONPathFinder::GetCost(const FSVONLink& Start, const FSVONLink& Target)
{
	float Cost = 0.0f;

	// Unit Cost implementation
	if (Settings.bUseUnitCost)
		Cost = Settings.UnitCost;
	else
	{
		FVector StartLocation(0.f), EndLocation(0.f);
		const FSVONNode& StartNode = Volume.GetNode(Start);
		const FSVONNode& EndNode = Volume.GetNode(Target);
		Volume.GetLinkLocation(Start, StartLocation);
		Volume.GetLinkLocation(Target, EndLocation);
		Cost = (StartLocation - EndLocation).Size();
	}

	Cost *= (1.0f - (StaticCast<float>(Target.LayerIndex) / StaticCast<float>(Volume.GetNumLayers())) * Settings.NodeSizeCompensation);

	return Cost;
}

void FSVONPathFinder::ProcessLink(const FSVONLink& Neighbor)
{
    if (Neighbor.IsValid())
	{
		if (ClosedSet.Contains(Neighbor))
			return;

		if (!OpenSet.Contains(Neighbor))
		{
			OpenSet.Add(Neighbor);
            if (Settings.bDebugOpenNodes)
            {
                FVector Location;
                Volume.GetLinkLocation(Neighbor, Location);
                Settings.DebugPoints.Add(Location);
            }
		}

		float GScore = FLT_MAX;
		if (this->GScore.Contains(Current))
			GScore = this->GScore[Current] + GetCost(Current, Neighbor);
		else
			this->GScore.Add(Current, FLT_MAX);

		if (GScore >= (this->GScore.Contains(Neighbor) ? this->GScore[Neighbor] : FLT_MAX))
			return;

		CameFrom.Add(Neighbor, Current);
	    this->GScore.Add(Neighbor, GScore);
	    this->FScore.Add(Neighbor, this->GScore[Neighbor] + (Settings.WeightEstimate * HeuristicScore(Neighbor, Goal)));
	}
}

void FSVONPathFinder::BuildPath(TMap<FSVONLink, FSVONLink>& CameFrom, FSVONLink Current, const FVector& StartLocation, const FVector& TargetLocation, FNavPathSharedPtr* OutPath)
{
	FVector Location;
	TArray<FVector> Points;
	if (!OutPath || !OutPath->IsValid())
		return;

	while (CameFrom.Contains(Current) && !(Current == CameFrom[Current]))
	{
		Current = CameFrom[Current];
		Volume.GetLinkLocation(Current, Location);
        Points.Add(Location);
	}

	if (Points.Num() > 1)
	{
		Points[0] = TargetLocation;
		Points[Points.Num() - 1] = StartLocation;
	}

	Smooth_Chaikin(Points, Settings.SmoothingIterations);

    for (auto i = Points.Num() - 1; i >= 0; i--)
        OutPath->Get()->GetPathPoints().Add(Points[i]);
}

void FSVONPathFinder::Smooth_Chaikin(TArray<FVector>& Points, int NumIterations)
{
	for (auto i = 0; i < NumIterations; i++)
	{
		for (int j = 0; j < Points.Num() - 1; j += 2)
		{
			FVector StartLocation = Points[j];
			FVector EndLocation = Points[j + 1];
			if (j > 0)
                Points[j] = FMath::Lerp(StartLocation, EndLocation, 0.25f);

			FVector SecondValue = FMath::Lerp(StartLocation, EndLocation, 0.75f);
			Points.Insert(SecondValue, j + 1);
		}
		Points.RemoveAt(Points.Num() - 1);
	}
}