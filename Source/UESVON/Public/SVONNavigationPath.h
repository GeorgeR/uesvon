#pragma once

#include "CoreMinimal.h"

#include "SVONNavigationPath.generated.h"

UENUM(BlueprintType)
enum class ESVONPathCostType : uint8
{
	SPCT_Manhattan  UMETA(DisplayName = "Manhattan"),
	SPCT_Euclidean  UMETA(DisplayName = "Euclidean")
};

struct FNavigationPath;
class ASVONVolumeActor;

struct UESVON_API FSVONPathPoint
{
public:
	FVector Location;
	int32 Layer;

	FSVONPathPoint()
		: Location(FVector::ZeroVector),
		Layer(-1) { }

	FSVONPathPoint(const FVector& Location, int32 Layer)
		: Location(Location),
		Layer(Layer) { }
};

struct UESVON_API FSVONNavigationPath
{
public:
	FORCEINLINE const TArray<FSVONPathPoint>& GetPathPoints() const { return Points; };
	FORCEINLINE TArray<FSVONPathPoint>& GetPathPoints() { return Points; };

	void AddPoint(const FSVONPathPoint& Point);
	void ResetForRepath();

	FORCEINLINE const bool IsReady() const { return bIsReady; }
	FORCEINLINE void SetIsReady(const bool IsReady) { this->bIsReady = IsReady; }

	void CreateNavigationPath(FNavigationPath& OutPath);

	void DebugDraw(UWorld* World, const ASVONVolumeActor& Volume);

protected:
	bool bIsReady;
	TArray<FSVONPathPoint> Points;
};
