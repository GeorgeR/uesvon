#include "SVONVolumeActor.h"

#include "Engine/CollisionProfile.h"
#include "Components/BrushComponent.h"
#include "Components/LineBatchComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include <chrono>

using namespace std::chrono;

ASVONVolumeActor::ASVONVolumeActor()
	: DebugLocation(FVector::ZeroVector)
{
	GetBrushComponent()->Mobility = EComponentMobility::Static;

	BrushColor = FColor(255, 255, 255, 255);

	bColored = true;

    const auto Bounds = AActor::GetComponentsBoundingBox(true);
	Bounds.GetCenterAndExtents(Origin, Extent);
}

#if WITH_EDITOR
void ASVONVolumeActor::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{ 
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ASVONVolumeActor::PostEditUndo()
{
	Super::PostEditUndo();
}

void ASVONVolumeActor::OnPostShapeChanged()
{

}
#endif // WITH_EDITOR

/************************************************************************/
/* Regenerates the Sparse Voxel Octree Navmesh                          */
/************************************************************************/
bool ASVONVolumeActor::Generate()
{
#if WITH_EDITOR
	GetWorld()->PersistentLineBatcher->SetComponentTickEnabled(false);

    const auto PlayerController = GetWorld()->GetFirstPlayerController();
	if (PlayerController)
		DebugLocation = PlayerController->GetPawn()->GetActorLocation();
	else if (GetWorld()->ViewLocationsRenderedLastFrame.Num() > 0)
		DebugLocation = GetWorld()->ViewLocationsRenderedLastFrame[0];

	FlushPersistentDebugLines(GetWorld());

	SetupVolume();
#endif

#if WITH_EDITOR
	// Setup timing
    const auto StartTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
#endif

	// Clear data (for now)
	BlockedIndices.Empty();
	Data.Layers.Empty();

	NumLayers = VoxelPower + 1;

	// Rasterize at LayerIndex 1
	FirstPassRasterize();

	// Allocate the Leaf Node data
	Data.LeafNodes.Empty();
	Data.LeafNodes.AddDefaulted(BlockedIndices[0].Num() * 8 * 0.25f);

	// Add layers
	for (auto i = 0; i < NumLayers; i++)
		Data.Layers.Emplace();

	// Rasterize LayerIndex, bottom up, adding parent/child links
	for (auto i = 0; i < NumLayers; i++)
		RasterizeLayer(i);

	// Now traverse down, adding Neighbor links
	for (auto i = NumLayers - 2; i >= 0; i--)
		BuildNeighborLinks(i);

#if WITH_EDITOR
    const auto BuildTime = (duration_cast<milliseconds>(system_clock::now().time_since_epoch()) - StartTime).count();

    auto TotalNodeCount = 0;
	for (auto i = 0; i < NumLayers; i++)
		TotalNodeCount += Data.Layers[i].Num();

	auto TotalBytes = sizeof(FSVONNode) * TotalNodeCount;
	TotalBytes += sizeof(FSVONLeafNode) * Data.LeafNodes.Num();

	UE_LOG(UESVON, Display, TEXT("Generation Time : %d"), StaticCast<int32>(BuildTime));
	UE_LOG(UESVON, Display, TEXT("Total Layers-Nodes : %d-%d"), NumLayers, TotalNodeCount);
	UE_LOG(UESVON, Display, TEXT("Total Leaf Nodes : %d"), Data.LeafNodes.Num());
	UE_LOG(UESVON, Display, TEXT("Total Size (bytes): %d"), StaticCast<int32>(TotalBytes));
#endif

	NumBytes = Data.GetSize();

	return true;
}

void ASVONVolumeActor::SetupVolume()
{
    const auto Bounds = GetComponentsBoundingBox(true);
	Bounds.GetCenterAndExtents(Origin, Extent);
}

bool ASVONVolumeActor::FirstPassRasterize()
{
	// Add the first LayerIndex of blocking
    BlockedIndices.Emplace();

    const auto NumNodes = GetNodesInLayer(1);
	for (auto i = 0; i < NumNodes; i++)
	{
		FVector Location;
		GetNodeLocation(1, i, Location);

		static FName NAME_TraceTag(TEXT("SVONFirstPassRasterize"));

		FCollisionQueryParams Params;
		Params.bFindInitialOverlaps = true;
		Params.bTraceComplex = false;
		Params.TraceTag = NAME_TraceTag;
		if (GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, CollisionChannel, FCollisionShape::MakeBox(FVector(GetVoxelSize(1) * 0.5f)), Params))
			BlockedIndices[0].Add(i);
	}

    auto LayerIndex = 0;
	while (BlockedIndices[LayerIndex].Num() > 1)
	{
		// Add a new LayerIndex to structure
		BlockedIndices.Emplace();

		// Add any parent morton codes to the new LayerIndex
		for (auto& Code : BlockedIndices[LayerIndex])
			BlockedIndices[LayerIndex + 1].Add(Code >> 3);

		LayerIndex++;
	}

	return true;
}

bool ASVONVolumeActor::GetNodeLocation(const FLayerIndex LayerIndex, const FMortonCode Code, FVector& OutLocation) const
{
    const auto VoxelSize = GetVoxelSize(LayerIndex);
	uint_fast32_t X, Y, Z;
	morton3D_64_decode(Code, X, Y, Z);

	OutLocation = Origin - Extent + FVector(X * VoxelSize, Y * VoxelSize, Z * VoxelSize) + FVector(VoxelSize * 0.5f);
	
    return true;
}

// Gets the Location of a given link. Returns true if the link is open, false if blocked
bool ASVONVolumeActor::GetLinkLocation(const FSVONLink& Link, FVector& OutLocation) const
{
	const auto& Node = GetLayer(Link.LayerIndex)[Link.NodeIndex];

	GetNodeLocation(Link.LayerIndex, Node.Code, OutLocation);

	// If this is LayerIndex 0, and there are valid children
	if (Link.LayerIndex == 0 && Node.FirstChild.IsValid())
	{
        const auto VoxelSize = GetVoxelSize(0);
		uint_fast32_t X, Y, Z;
		morton3D_64_decode(Link.SubNodeIndex, X,Y,Z);

		OutLocation += FVector(X * VoxelSize * 0.25f, Y * VoxelSize * 0.25f, Z * VoxelSize * 0.25f) - FVector(VoxelSize * 0.375);
		const auto& LeafNode = GetLeafNode(Node.FirstChild.NodeIndex);
        const auto bIsBlocked = LeafNode.GetNode(Link.SubNodeIndex);

		return !bIsBlocked;
	}

	return true;
}

bool ASVONVolumeActor::GetIndexForCode(const FLayerIndex LayerIndex, const FMortonCode Code, FNodeIndex& OutIndex) const
{
	const auto& Layer = GetLayer(LayerIndex);
	for (auto i = 0; i < Layer.Num(); i++)
	{
		if (Layer[i].Code == Code)
		{
			OutIndex = i;
			return true;
		}
	}

	return false;
}

const FSVONNode& ASVONVolumeActor::GetNode(const FSVONLink& Link) const
{
	// @todo: remove magic number
	if (Link.LayerIndex < 14)
		return GetLayer(Link.LayerIndex)[Link.NodeIndex];
	else
		return GetLayer(NumLayers - 1)[0];
}

const FSVONLeafNode& ASVONVolumeActor::GetLeafNode(const FNodeIndex Index) const
{
	return Data.LeafNodes[Index];
}

void ASVONVolumeActor::GetLeafNeighbors(const FSVONLink& Link, TArray<FSVONLink>& OutNeighbors) const
{
    const FMortonCode LeafIndex = Link.SubNodeIndex;
    const auto& Node = GetNode(Link);
	const auto& Leaf = GetLeafNode(Node.FirstChild.NodeIndex);

	// Get our starting co-ordinates
	uint_fast32_t X = 0, Y = 0, Z = 0;
	morton3D_64_decode(LeafIndex, X, Y, Z);

	for (auto i = 0; i < 6; i++)
	{
		// Need to switch to signed ints
		auto SX = X + FSVONStatics::Directions[i].X;
		auto SY = Y + FSVONStatics::Directions[i].Y;
		auto SZ = Z + FSVONStatics::Directions[i].Z;

		// If the Neighbor is in Bounds of this Leaf Node
		if (SX >= 0 && SX < 4 && SY >= 0 && SY < 4 && SZ >= 0 && SZ < 4)
		{
			auto Index = morton3D_64_encode(SX, SY, SZ);
			// If this Node is blocked, then no link in this direction, continue
			if (Leaf.GetNode(Index))
				continue;
			else // Otherwise, this is a valid link, add it
			{
				OutNeighbors.Emplace(0, Link.NodeIndex, Index);
				continue;
			}
		}
		else // the neighbors is out of Bounds, we need to find our Neighbor
		{
			const auto& NeighborLink = Node.Neighbors[i];
			const auto& NeighborNode = GetNode(NeighborLink);

			// If the Neighbor LayerIndex 0 has no Leaf nodes, just return it
			if (!NeighborNode.FirstChild.IsValid())
			{
				OutNeighbors.Add(NeighborLink);
				continue;
			}

			const auto& LeafNode = GetLeafNode(NeighborNode.FirstChild.NodeIndex);
			if (LeafNode.IsCompletelyBlocked())
			{
				// The Leaf Node is completely blocked, we don't return it
				continue;
			}
			else // Otherwise, we need to find the correct subnode
			{
				if (SX < 0)
					SX = 3;
				else if (SX > 3)
					SX = 0;
				else if (SY < 0)
					SY = 3;
				else if (SY > 3)
					SY = 0;
				else if (SZ < 0)
					SZ = 3;
				else if (SZ > 3)
					SZ = 0;

				auto SubNodeCode = morton3D_64_encode(SX, SY, SZ);

				// Only return the Neighbor if it isn't blocked!
				if (!LeafNode.GetNode(SubNodeCode))
					OutNeighbors.Emplace(0, NeighborNode.FirstChild.NodeIndex, SubNodeCode);
			}
		}
	}
}

void ASVONVolumeActor::GetNeighbors(const FSVONLink& Link, TArray<FSVONLink>& OutNeighbors) const
{
	const auto& Node = GetNode(Link);
	for (auto i = 0; i < 6; i++)
	{
		const auto& NeighborLink = Node.Neighbors[i];
		if (!NeighborLink.IsValid())
			continue;

		const auto& Neighbor = GetNode(NeighborLink);

		// If the Neighbor has no children, it's empty, we just use it
		if (!Neighbor.HasChildren())
		{
			OutNeighbors.Add(NeighborLink);
			continue;
		}

		// If the node has children, we need to look down the tree to see which children we want to add to the neighbour set
		// Start working set, and put the link into it
		TArray<FSVONLink> WorkingSet;
		WorkingSet.Push(NeighborLink);

		while (WorkingSet.Num() > 0)
		{
			auto CurrentLink = WorkingSet.Pop();
			const auto& CurrentNode = GetNode(CurrentLink);

			// If the node has no children, it's clear, so add to neighbors and continue
			if (!CurrentNode.HasChildren())
			{
				OutNeighbors.Add(NeighborLink);
				continue;
			}

			// Otherwise it has children
			if (CurrentLink.GetLayerIndex() > 0)
			{
				for (const auto& ChildIdx : FSVONStatics::DirectionalChildOffsets[i])
				{
					auto ChildLink = CurrentNode.FirstChild;
					ChildLink.NodeIndex += ChildIdx;
					const auto& ChildNode = GetNode(ChildLink);

					if (ChildNode.HasChildren())
						WorkingSet.Emplace(ChildLink);
					else
						OutNeighbors.Emplace(ChildLink);
				}
			}
			else
			{
				for (const auto& LeafIdx : FSVONStatics::DirectionalLeafChildOffsets[i])
				{
					auto LeafLink = Neighbor.FirstChild;
					const auto& LeafNode = GetLeafNode(LeafLink.NodeIndex);
					LeafLink.SubNodeIndex = LeafIdx;

					if (!LeafNode.GetNode(LeafIdx))
						OutNeighbors.Emplace(LeafLink);
				}
			}
		}
	}
}

void ASVONVolumeActor::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (GenerationStrategy == ESVOGenerationStrategy::SGS_UseBaked)
	{
		Ar << Data;

		NumLayers = Data.Layers.Num();
		NumBytes = Data.GetSize();
	}
}

float ASVONVolumeActor::GetVoxelSize(const FLayerIndex LayerIndex) const
{
	return (Extent.X / FMath::Pow(2, VoxelPower)) * (FMath::Pow(2.0f, LayerIndex + 1));
}

bool ASVONVolumeActor::IsReadyForNavigation() const
{
	return bIsReadyForNavigation;
}

int32 ASVONVolumeActor::GetNodesInLayer(const FLayerIndex LayerIndex) const
{
	return FMath::Pow(FMath::Pow(2, (VoxelPower - (LayerIndex))), 3);
}

int32 ASVONVolumeActor::GetNodesPerSide(const FLayerIndex LayerIndex) const
{
	return FMath::Pow(2, (VoxelPower - (LayerIndex)));
}

void ASVONVolumeActor::BeginPlay()
{
	if (!bIsReadyForNavigation && GenerationStrategy == ESVOGenerationStrategy::SGS_GenerateOnBeginPlay)
		Generate();
	else
		SetupVolume();

	bIsReadyForNavigation = true;
}

void ASVONVolumeActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
}

void ASVONVolumeActor::PostUnregisterAllComponents()
{
	Super::PostUnregisterAllComponents();
}

void ASVONVolumeActor::BuildNeighborLinks(const FLayerIndex LayerIndex)
{
	auto& Layer = GetLayer(LayerIndex);
	auto SearchLayerIndex = LayerIndex;

	// For each Node
	for (FNodeIndex i = 0; i < Layer.Num(); i++)
	{
		auto& Node = Layer[i];

		// Get our world co-ordinate
		uint_fast32_t X, Y, Z;
		morton3D_64_decode(Node.Code, X, Y, Z);

		FNodeIndex BacktrackIndex = -1;
		FNodeIndex Index = i;
		FVector NodeLocation;
		GetNodeLocation(LayerIndex, Node.Code, NodeLocation);

		// For each direction
		for (auto DirectionIndex = 0; DirectionIndex < 6; DirectionIndex++)
		{
			auto& LinkToUpdate = Node.Neighbors[DirectionIndex];
			BacktrackIndex = Index;

			while (!FindLinkInDirection(SearchLayerIndex, Index, DirectionIndex, LinkToUpdate, NodeLocation)
				&& LayerIndex < Data.Layers.Num() - 2)
			{
				auto& Parent = GetLayer(SearchLayerIndex)[Index].Parent;
				if (Parent.IsValid())
				{
					Index = Parent.NodeIndex;
					SearchLayerIndex = Parent.LayerIndex;
				}
				else
				{
					SearchLayerIndex++;
					GetIndexForCode(SearchLayerIndex, Node.Code >> 3, Index);
				}
			}

			Index = BacktrackIndex;
			SearchLayerIndex = LayerIndex;
		}
	}
}

bool ASVONVolumeActor::FindLinkInDirection(const FLayerIndex LayerIndex, const FNodeIndex NodeIndex, const uint8 Direction, FSVONLink& OutLinkToUpdate, FVector& OutStartLocationForDebug)
{
    const auto MaxCoord = GetNodesPerSide(LayerIndex);
	auto& Node = GetLayer(LayerIndex)[NodeIndex];
	auto& Layer = GetLayer(LayerIndex);

	// Get our world co-ordinate
	uint_fast32_t X = 0, Y = 0, Z = 0;
	morton3D_64_decode(Node.Code, X, Y, Z);

	int32 SX = X, SY = Y, SZ = Z;

	// Add the direction
	SX += FSVONStatics::Directions[Direction].X;
	SY += FSVONStatics::Directions[Direction].Y;
	SZ += FSVONStatics::Directions[Direction].Z;

	// If the coords are out of Bounds, the link is invalid.
	if (SX < 0 || SX >= MaxCoord || SY < 0 || SY >= MaxCoord || SZ < 0 || SZ >= MaxCoord)
	{
		OutLinkToUpdate.SetInvalid();
		if (bShowNeighborLinks && IsInDebugRange(OutStartLocationForDebug))
		{
			FVector StartLocation;
			GetNodeLocation(LayerIndex, Node.Code, StartLocation);

            const auto EndLocation = StartLocation + (FVector(FSVONStatics::Directions[Direction]) * 100.f);
			DrawDebugLine(GetWorld(), OutStartLocationForDebug, EndLocation, FColor::Red, true, -1.f, 0, .0f);
		}

		return true;
	}

	X = SX; Y = SY; Z = SZ;

	// Get the morton Code for the direction
    const auto Code = morton3D_64_encode(X, Y, Z);
    const auto bIsHigher = Code > Node.Code;
	auto NodeDelta = (bIsHigher ?  1 : - 1);

	while ((NodeIndex + NodeDelta) < Layer.Num() && NodeIndex + NodeDelta >= 0)
	{
		// This is the Node we're looking for
        if (Layer[NodeIndex + NodeDelta].Code == Code)
        {
            const auto& NodeOnLayer = Layer[NodeIndex + NodeDelta];
            // This is a Leaf Node
            if (LayerIndex == 0 && NodeOnLayer.HasChildren())
            {
                // Set invalid link if the Leaf Node is completely blocked, no point linking to it
                if (GetLeafNode(NodeOnLayer.FirstChild.NodeIndex).IsCompletelyBlocked())
                {
                    OutLinkToUpdate.SetInvalid();
                    return true;
                }
            }
            // Otherwise, use this link
            OutLinkToUpdate.LayerIndex = LayerIndex;
            check(NodeIndex + NodeDelta < Layer.Num());
            OutLinkToUpdate.NodeIndex = NodeIndex + NodeDelta;

            if (bShowNeighborLinks && IsInDebugRange(OutStartLocationForDebug))
            {
                FVector EndLocation;
                GetNodeLocation(LayerIndex, Code, EndLocation);
                DrawDebugLine(GetWorld(), OutStartLocationForDebug, EndLocation, FSVONStatics::LinkColors[LayerIndex], true, -1.f, 0, .0f);
            }

            return true;
        }
        // If we've passed the Code we're looking for, it's not on this LayerIndex
        else if ((bIsHigher && Layer[NodeIndex + NodeDelta].Code > Code) || (!bIsHigher && Layer[NodeIndex + NodeDelta].Code < Code))
            return false;

		NodeDelta += (bIsHigher ? 1 : -1);
	}

	// I'm not entirely sure if it's valid to reach the end? Hmmm...
	return false;
}

void ASVONVolumeActor::RasterizeLeafNode(FVector& InOrigin, const FNodeIndex LeafIndex)
{
	// @todo: remove magic number
	for (auto i = 0; i < 64; i++)
	{
		uint_fast32_t X, Y, Z;
		morton3D_64_decode(i, X, Y, Z);

        const auto LeafVoxelSize = GetVoxelSize(0) * 0.25f;
        auto Location = InOrigin + FVector(X * LeafVoxelSize, Y * LeafVoxelSize, Z * LeafVoxelSize) + FVector(LeafVoxelSize * 0.5f);

		if (LeafIndex >= Data.LeafNodes.Num() - 1)
			Data.LeafNodes.AddDefaulted(1);

		if(IsBlocked(Location, LeafVoxelSize * 0.5f))
		{
			Data.LeafNodes[LeafIndex].SetNode(i);

			if (bShowLeafVoxels && IsInDebugRange(Location))
				DrawDebugBox(GetWorld(), Location, FVector(LeafVoxelSize * 0.5f), FQuat::Identity, FColor::Red, true, -1.f, 0, .0f);
		}
	}
}

TArray<FSVONNode>& ASVONVolumeActor::GetLayer(const FLayerIndex LayerIndex)
{
	return Data.Layers[LayerIndex];
}

const TArray<FSVONNode>& ASVONVolumeActor::GetLayer(const FLayerIndex LayerIndex) const
{
	return Data.Layers[LayerIndex];
}

// Check for blocking...using this cached set for each LayerIndex for now for fast lookups
bool ASVONVolumeActor::IsAnyMemberBlocked(const FLayerIndex LayerIndex, const FMortonCode Code)
{
    const auto ParentCode = Code >> 3;
	if (LayerIndex == BlockedIndices.Num())
		return true;

	// The parent of this Code is blocked
	if (BlockedIndices[LayerIndex].Contains(ParentCode))
		return true;

	return false;
}

bool ASVONVolumeActor::IsBlocked(const FVector& Location, const float Size) const
{
	static FName NAME_TraceTag = TEXT("SVONLeafRasterize");

	FCollisionQueryParams Params;
	Params.bFindInitialOverlaps = true;
	Params.bTraceComplex = false;
	Params.TraceTag = NAME_TraceTag;

	return GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, CollisionChannel, FCollisionShape::MakeBox(FVector(Size + Clearance)), Params);
}

bool ASVONVolumeActor::IsInDebugRange(const FVector& Location) const
{
	return FVector::DistSquared(DebugLocation, Location) < DebugDistance * DebugDistance;
}

bool ASVONVolumeActor::SetNeighbor(const FLayerIndex LayerIndex, const FNodeIndex ArrayIndex, const EDirection Direction)
{
	return false;
}

void ASVONVolumeActor::RasterizeLayer(const FLayerIndex LayerIndex)
{
    FNodeIndex LeafIndex = 0;
    // LayerIndex 0 Leaf nodes are special
    if (LayerIndex == 0)
    {
        // Run through all our coordinates
        const auto NumNodes = GetNodesInLayer(LayerIndex);
        for (auto i = 0; i < NumNodes; i++)
        {
            auto Index = i;

            // If we know this Node needs to be added, from the low res first pass
            if (BlockedIndices[0].Contains(i >> 3))
            {
                // Add a Node
                Index = GetLayer(LayerIndex).Emplace();
                auto& Node = GetLayer(LayerIndex)[Index];

                // Set my Code and Location
                Node.Code = i;

                FVector NodeLocation;
                GetNodeLocation(LayerIndex, Node.Code, NodeLocation);

                // Debug stuff
                if (bShowMortonCodes && IsInDebugRange(NodeLocation))
                    DrawDebugString(GetWorld(), NodeLocation, FString::FromInt(Node.Code), nullptr, FSVONStatics::LayerColors[LayerIndex], -1, false);

                if (bShowVoxels && IsInDebugRange(NodeLocation))
                    DrawDebugBox(GetWorld(), NodeLocation, FVector(GetVoxelSize(LayerIndex) * 0.5f), FQuat::Identity, FSVONStatics::LayerColors[LayerIndex], true, -1.f, 0, .0f);

                // Now check if we have any blocking, and search Leaf nodes
                FVector Location;
                GetNodeLocation(0, i, Location);

				static FName NAME_TraceTag = TEXT("SVONRasterize");

                FCollisionQueryParams Params;
                Params.bFindInitialOverlaps = true;
                Params.bTraceComplex = false;
                Params.TraceTag = NAME_TraceTag;

                if (IsBlocked(Location, GetVoxelSize(0) * 0.5f))
                {
                    // Rasterize my Leaf nodes
                    auto LeafOrigin = NodeLocation - (FVector(GetVoxelSize(LayerIndex) * 0.5f));
                    RasterizeLeafNode(LeafOrigin, LeafIndex);
                    Node.FirstChild.LayerIndex = 0;
                    Node.FirstChild.NodeIndex = LeafIndex;
                    Node.FirstChild.SubNodeIndex = 0;
                    LeafIndex++;
                }
                else
                {
                    Data.LeafNodes.AddDefaulted(1);
                    LeafIndex++;
                    Node.FirstChild.SetInvalid();
                }
            }
        }
    }
    // Deal with the other layers
    else if (GetLayer(LayerIndex - 1).Num() > 1)
    {
        auto NodeCounter = 0;
        const int32 NumNodes = GetNodesInLayer(LayerIndex);
        for (auto i = 0; i < NumNodes; i++)
        {
            // Do we have any blocking children, or siblings?
            // Remember we must have 8 children per parent
            if (IsAnyMemberBlocked(LayerIndex, i))
            {
                // Add a Node
                const auto Index = GetLayer(LayerIndex).Emplace();
                NodeCounter++;
                auto& Node = GetLayer(LayerIndex)[Index];

                // Set details
                Node.Code = i;
                auto ChildIndex = 0;
                if (GetIndexForCode(LayerIndex - 1, Node.Code << 3, ChildIndex))
                {
                    // Set parent->child links
                    Node.FirstChild.LayerIndex = LayerIndex - 1;
                    Node.FirstChild.NodeIndex = ChildIndex;
                    // Set child->parent links, this can probably be done smarter, as we're duplicating work here
                    for (auto j = 0; j < 8; j++)
                    {
                        GetLayer(Node.FirstChild.LayerIndex)[Node.FirstChild.NodeIndex + j].Parent.LayerIndex = LayerIndex;
                        GetLayer(Node.FirstChild.LayerIndex)[Node.FirstChild.NodeIndex + j].Parent.NodeIndex = Index;
                    }

                    if (bShowParentChildLinks) // Debug all the things
                    {
                        FVector StartLocation, EndLocation;
                        GetNodeLocation(LayerIndex, Node.Code, StartLocation);
                        GetNodeLocation(LayerIndex - 1, Node.Code << 3, EndLocation);
                        DrawDebugDirectionalArrow(GetWorld(), StartLocation, EndLocation, 0.f, FSVONStatics::LinkColors[LayerIndex], true);
                    }
                }
                else
                    Node.FirstChild.SetInvalid();

                if (bShowMortonCodes || bShowVoxels)
                {
                    FVector NodeLocation;
                    GetNodeLocation(LayerIndex, i, NodeLocation);

                    // Debug stuff
                    if (bShowVoxels && IsInDebugRange(NodeLocation))
                        DrawDebugBox(GetWorld(), NodeLocation, FVector(GetVoxelSize(LayerIndex) * 0.5f), FQuat::Identity, FSVONStatics::LayerColors[LayerIndex], true, -1.f, 0, .0f);

                    if (bShowMortonCodes && IsInDebugRange(NodeLocation))
                        DrawDebugString(GetWorld(), NodeLocation, FString::FromInt(Node.Code), nullptr, FSVONStatics::LayerColors[LayerIndex], -1, false);
                }
            }
        }
    }
}
