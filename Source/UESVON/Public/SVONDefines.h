#pragma once

#include "CoreMinimal.h"

typedef uint8 FLayerIndex;
typedef int32 FNodeIndex;
typedef uint8 FSubNodeIndex;
typedef uint_fast64_t FMortonCode;
typedef uint_fast32_t FPosInt;

UENUM(BlueprintType)		
enum class EBuildTrigger : uint8
{
	BTOnEdit	UMETA(DisplayName = "On Edit"),
	BTManual 	UMETA(DisplayName = "Manual")
};

enum class EDirection : uint8
{
	DPositiveX  UMETA(DisplayName = "+X"), 
    DNegativeX  UMETA(DisplayName = "-X"),
    DPositiveY  UMETA(DisplayName = "+Y"),
    DNegativeY  UMETA(DisplayName = "-Y"),
    DPositiveZ  UMETA(DisplayName = "+Z"),
    DNegativeZ  UMETA(DisplayName = "-Z")
};

#define LEAF_LAYER_INDEX 14;

class UESVON_API FSVONStatics
{
public:
	static const FIntVector Directions[];
	static const FNodeIndex DirectionChildOffsets[6][4];
	static const FNodeIndex DirectionLeafChildOffsets[6][16];
	static const FColor LayerColors[];
	static const FColor LinkColors[];
};