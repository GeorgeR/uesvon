#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"

#include "SVONDefines.h"
#include "SVONNode.h"
#include "SVONLeafNode.h"
#include "SVONData.h"
#include "UESVON.h"

#include "SVONVolume.generated.h"

UCLASS(HideCategories = (Tags, Cooking, Actor, HLOD, Mobile, LOD))
class UESVON_API ASVONVolume 
    : public AVolume
{
	GENERATED_BODY()
	
public:
    ASVONVolume();

	virtual void BeginPlay() override;

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
#endif // WITH_EDITOR

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

	bool Generate();

	const FVector& GetOrigin() const { return Origin; }
	const FVector& GetExtent() const { return Extent; }
	const uint8 GetNumLayers() const { return NumLayers; }
	const TArray<FSVONNode>& GetLayer(FLayerIndex Layer) const;
	float GetVoxelSize(FLayerIndex Layer) const;

	bool IsReadyForNavigation();
	
	bool GetLinkLocation(const FSVONLink& Link, FVector& OutLocation) const;
	bool GetNodeLocation(FLayerIndex Layer, FMortonCode Code, FVector& OutLocation) const;
	const FSVONNode& GetNode(const FSVONLink& Link) const;
	const FSVONLeafNode& GetLeafNode(FNodeIndex Index) const;

	void GetLeafNeighbors(const FSVONLink& Link, TArray<FSVONLink>& OutNeighbors) const;
	void GetNeighbors(const FSVONLink& Link, TArray<FSVONLink>& OutNeighbors) const;

private:
	bool bIsReadyForNavigation = false;

	FVector Origin;
	FVector Extent;

	uint8 NumLayers = 0;
	
	FSVONData Data;

	// First pass rasterize results
	TArray<TSet<FMortonCode>> BlockedIndices;

	TArray<FSVONNode>& GetLayer(FLayerIndex Layer);

	bool FirstPassRasterize();
	void RasterizeLayer(FLayerIndex Layer);

	int32 GetNodesInLayer(FLayerIndex Layer);
	int32 GetNodesPerSide(FLayerIndex Layer);

	bool GetIndexForCode(FLayerIndex Layer, FMortonCode Code, FNodeIndex& OutIndex) const;

    void BuildNeighborLinks(FLayerIndex Layer);
	bool FindLinkInDirection(FLayerIndex Layer, const FNodeIndex NodeIndex, uint8 Direction, FSVONLink& OutLinkToUpdate, FVector& OutStartLocationForDebug);
	void RasterizeLeafNode(FVector& Origin, FNodeIndex LeafIndex);
	bool SetNeighbor(const FLayerIndex Layer, const FNodeIndex ArrayIndex, const EDirection Direction);

	bool IsAnyMemberBlocked(FLayerIndex Layer, FMortonCode Code);

	bool IsBlocked(const FVector& Location, const float Size) const;
};