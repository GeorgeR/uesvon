#include "SVONPath.h"

void FSVONPath::AddPoint(const FVector& Point)
{
	Points.Add(Point);
}

void FSVONPath::ResetPath()
{
	Points.Empty();
}

void FSVONPath::DebugDraw(UWorld* World)
{
	for (auto i = 0; i < Points.Num(); i++)
	{
        auto& Point = Points[i];
		if (i < Points.Num() - 1)
		{
			FVector OffSet(0.f);
			//if (i == 0)
			//	//offSet.Z -= 300.f;
			
			DrawDebugSphere(World, Point + OffSet, 30.f, 20, FColor::Cyan, true, -1.f, 0, 100.f);
			DrawDebugLine(World, Point + OffSet, Points[i+1], FColor::Cyan, true, -1.f, 0, 100.f);
		}
	}
}