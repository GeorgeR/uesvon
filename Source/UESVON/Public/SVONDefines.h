#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"

typedef uint8 FLayerIndex;
typedef int32 FNodeIndex;
typedef uint8 FSubNodeIndex;
typedef uint_fast64_t FMortonCode;
typedef uint_fast32_t FPosInt;

UENUM(BlueprintType)		
enum class EBuildTrigger : uint8
{
	BT_OnEdit	UMETA(DisplayName = "On Edit"),
	BT_Manual 	UMETA(DisplayName = "Manual")
};

enum class EDirection : uint8
{
	D_PositiveX  UMETA(DisplayName = "+X"), 
    D_NegativeX  UMETA(DisplayName = "-X"),
    D_PositiveY  UMETA(DisplayName = "+Y"),
    D_NegativeY  UMETA(DisplayName = "-Y"),
    D_PositiveZ  UMETA(DisplayName = "+Z"),
    D_NegativeZ  UMETA(DisplayName = "-Z")
};

#define LEAF_LAYER_INDEX 14;

class UESVON_API FSVONStatics
{
public:
	static const FIntVector Directions[];
	static const FNodeIndex DirectionalChildOffsets[6][4];
	static const FNodeIndex DirectionalLeafChildOffsets[6][16];
	static const FColor LayerColors[];
	static const FColor LinkColors[];
};

UENUM(BlueprintType)
namespace ESVONPathfindingRequestResult
{
	enum Type
	{
		SPRR_Failed				UMETA(DisplayName = "Failed"), // Something went wrong
		SPRR_ReadyToPath		UMETA(DisplayName = "Ready to Path"), // Pre-reqs satisfied
		SPRR_AlreadyAtGoal		UMETA(DisplayName = "Already at Goal"), // No need to move
		SPRR_Deferred			UMETA(DisplayName = "Deferred"), // Passed request to another thread, need to wait
		SPRR_Success			UMETA(DisplayName = "Success") // it worked!
	};
}

struct UESVON_API FSVONPathfindingRequestResult
{
public:
	FAIRequestID MoveId;
	TEnumAsByte<ESVONPathfindingRequestResult::Type> Code;

	FSVONPathfindingRequestResult()
		: MoveId(FAIRequestID::InvalidRequest),
		Code(ESVONPathfindingRequestResult::SPRR_Failed) {}

	operator ESVONPathfindingRequestResult::Type() const { return Code; }
};
