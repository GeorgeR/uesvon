#include "SVONNavigationPath.h"

#include "DrawDebugHelpers.h"
#include "Debug/DebugDrawService.h"
#include "SVONVolumeActor.h"
#include "NavigationData.h"

void FSVONNavigationPath::AddPoint(const FSVONPathPoint& Point)
{
	Points.Add(Point);
}

void FSVONNavigationPath::ResetForRepath()
{
	Points.Empty();
}

void FSVONNavigationPath::CreateNavigationPath(FNavigationPath& OutPath)
{
	for (const auto& Point : Points)
		OutPath.GetPathPoints().Add(Point.Location);
}

void FSVONNavigationPath::DebugDraw(UWorld* World, const ASVONVolumeActor& Volume)
{
	for (auto i = 0; i < Points.Num(); i++)
	{
		auto& Point = Points[i];
		if (i < Points.Num() - 1)
		{
			FVector Offset(0.0f);
			auto Size = Point.Layer == 0 ? Volume.GetVoxelSize(Point.Layer) * 0.25f : Volume.GetVoxelSize(Point.Layer) * 0.5f;
			DrawDebugBox(World, Point.Location, FVector(Size), FSVONStatics::LinkColors[Point.Layer], true, -1.0f, 0, 30.0f);
			DrawDebugSphere(World, Point.Location + Offset, 30.0f, 20, FColor::Cyan, true, -1.0f, 0, 100.0f);
		}
	}
}
