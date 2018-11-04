#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class ESVONPathCostType : uint8
{
	SPCT_Manhattan  UMETA(DisplayName = "Manhattan"),
	SPCT_Euclidean  UMETA(DisplayName = "Euclidean")
};

struct UESVON_API FSVONPath
{
protected:
	TArray<FVector> Points;

public:
	void AddPoint(const FVector& aPoint);
	void ResetPath();

	void DebugDraw(UWorld* aWorld);

	FORCEINLINE const TArray<FVector>& GetPoints() const { return Points; };
};