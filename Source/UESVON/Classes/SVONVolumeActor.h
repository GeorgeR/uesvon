#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"

#include "SVONDefines.h"
#include "SVONNode.h"
#include "SVONLeafNode.h"
#include "SVONData.h"
#include "UESVON.h"

#include "SVONVolumeActor.generated.h"

UENUM(BlueprintType)
enum class ESVOGenerationStrategy : uint8
{
	SGS_UseBaked				UMETA(DisplayName = "Use Baked"),
	SGS_GenerateOnBeginPlay		UMETA(DisplayName = "Generate on BeginPlay")
};

UCLASS(HideCategories = (Tags, Cooking, Actor, HLOD, Mobile, LOD))
class UESVON_API ASVONVolumeActor 
    : public AVolume
{
	GENERATED_BODY()
	
public:
    ASVONVolumeActor();

	virtual void BeginPlay() override;
	void Tick(float DeltaSeconds) override;

	//~ Begin AActor Interface
	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;
	//~ End AActor Interface

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	void OnPostShapeChanged();

	bool ShouldTickIfViewportsOnly() const override { return true; }
	//~ End UObject Interface
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVON")
	float DebugDistance = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVON")
	bool bShowVoxels = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVON")
	bool bShowLeafVoxels = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVON")
	bool bShowMortonCodes = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVON")
	bool bShowNeighborLinks = false;
	
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVON")
	bool bShowParentChildLinks = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVON")
	int32 VoxelPower = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVON")
	TEnumAsByte<ECollisionChannel> CollisionChannel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVON")
	float Clearance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVON")
	ESVOGenerationStrategy GenerationStrategy = ESVOGenerationStrategy::SGS_UseBaked;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SVON")
	uint8 NumLayers = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SVON")
	int32 NumBytes;

	bool Generate();
	bool ClearData();

	const FVector& GetOrigin() const { return Origin; }
	const FVector& GetExtent() const { return Extent; }
    uint8 GetNumLayers() const { return NumLayers; }
	const TArray<FSVONNode>& GetLayer(FLayerIndex LayerIndex) const;
	float GetVoxelSize(FLayerIndex LayerIndex) const;

	bool IsReadyForNavigation() const;
	
	bool GetLinkLocation(const FSVONLink& Link, FVector& OutLocation) const;
	bool GetNodeLocation(FLayerIndex LayerIndex, FMortonCode Code, FVector& OutLocation) const;
	const FSVONNode& GetNode(const FSVONLink& Link) const;
	const FSVONLeafNode& GetLeafNode(FNodeIndex Index) const;

	void GetLeafNeighbors(const FSVONLink& Link, TArray<FSVONLink>& OutNeighbors) const;
	void GetNeighbors(const FSVONLink& Link, TArray<FSVONLink>& OutNeighbors) const;

	virtual void Serialize(FArchive& Ar) override;

private:
	TArray<FTraceHandle> PendingTraces;

	bool bIsReadyForNavigation = false;
	FWorldAsyncTraceState AsyncTraceState;

	FVector Origin;
	FVector Extent;
	FVector DebugLocation;

	FSVONData Data;

	// First pass rasterize results
	TArray<TSet<FMortonCode>> BlockedIndices;

	TArray<FSVONNode>& GetLayer(FLayerIndex LayerIndex);

	void UpdateBounds();

	bool FirstPassRasterize();
	void RasterizeLayer(FLayerIndex LayerIndex);

	int32 GetNodesInLayer(FLayerIndex LayerIndex) const;
	int32 GetNodesPerSide(FLayerIndex LayerIndex) const;

	bool GetIndexForCode(FLayerIndex LayerIndex, FMortonCode Code, FNodeIndex& OutIndex) const;

    void BuildNeighborLinks(FLayerIndex LayerIndex);
	bool FindLinkInDirection(FLayerIndex LayerIndex, const FNodeIndex NodeIndex, uint8 Direction, FSVONLink& OutLinkToUpdate, FVector& OutStartLocationForDebug);
	void RasterizeLeafNode(FVector& InOrigin, FNodeIndex LeafIndex);
	bool SetNeighbor(const FLayerIndex LayerIndex, const FNodeIndex ArrayIndex, const EDirection Direction);

	bool IsAnyMemberBlocked(FLayerIndex LayerIndex, FMortonCode Code);

	bool IsBlocked(const FVector& Location, const float Size) const;

	bool IsInDebugRange(const FVector& Location) const;

	//TFuture<TSet<FMortonCode>> AsyncOverlapBlockingTestByChannel(const TArray<FVector>& Locations, ECollisionChannel InCollisionChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam);
	void StartBlockingTests(const TArray<FVector>& Locations, ECollisionChannel InCollisionChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam);
	void OnBlockingTestsComplete(const TArray<FTraceHandle>& InTraceHandles);
};

