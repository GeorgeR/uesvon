#include "SVONVolume.h"

#include "Engine/CollisionProfile.h"
#include "Components/BrushComponent.h"
#include "DrawDebugHelpers.h"
#include <chrono>

using namespace std::chrono;

ASVONVolume::ASVONVolume()
{
	GetBrushComponent()->Mobility = EComponentMobility::Static;

	BrushColor = FColor(255, 255, 255, 255);

	bColored = true;

	auto Bounds = GetComponentsBoundingBox(true);
	Bounds.GetCenterAndExtents(Origin, Extent);
}

#if WITH_EDITOR
void ASVONVolume::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{ 
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ASVONVolume::PostEditUndo()
{
	Super::PostEditUndo();
}

void ASVONVolume::OnPostShapeChanged()
{

}
#endif // WITH_EDITOR

/************************************************************************/
/* Regenerates the Sparse Voxel Octree Navmesh                          */
/************************************************************************/
bool ASVONVolume::Generate()
{
	FlushPersistentDebugLines(GetWorld());

	// Get Bounds and extent
	auto Bounbds = GetComponentsBoundingBox(true);
	Bounbds.GetCenterAndExtents(Origin, Extent);

	// Setup timing
	milliseconds startMs = duration_cast<milliseconds>(
		system_clock::now().time_since_epoch()
		);

	// Clear data (for now)
	BlockedIndices.Empty();
	Data.Layer.Empty();

	NumLayers = VoxelPower + 1;

	// Rasterize at LayerIndex 1
	FirstPassRasterize();

	// Allocate the Leaf Node data
	Data.LeafNodes.Empty();
	Data.LeafNodes.AddDefaulted(BlockedIndices[0].Num() * 8 * 0.25f);

	// Add layers
	for (auto i = 0; i < NumLayers; i++)
		Data.Layer.Emplace();

	// Rasterize LayerIndex, bottom up, adding parent/child links
	for (auto i = 0; i < NumLayers; i++)
		RasterizeLayer(i);

	// Now traverse down, adding Neighbor links
	for (auto i = NumLayers - 2; i >= 0; i--)
		BuildNeighborLinks(i);

	auto BuildTime = (duration_cast<milliseconds>(
		system_clock::now().time_since_epoch()
		) - startMs).count();

	int32 TotalNodeCount = 0;
	for (auto i = 0; i < NumLayers; i++)
		TotalNodeCount += Data.Layer[i].Num();

	auto TotalBytes = sizeof(FSVONNode) * TotalNodeCount;
	TotalBytes += sizeof(FSVONLeafNode) * Data.LeafNodes.Num();

	UE_LOG(UESVON, Display, TEXT("Generation Time : %d"), BuildTime);
	UE_LOG(UESVON, Display, TEXT("Total Layers-Nodes : %d-%d"), NumLayers, TotalNodeCount);
	UE_LOG(UESVON, Display, TEXT("Total Leaf Nodes : %d"), Data.LeafNodes.Num());
	UE_LOG(UESVON, Display, TEXT("Total Size (bytes): %d"), TotalBytes);

	return true;
}

bool ASVONVolume::FirstPassRasterize()
{
	// Add the first LayerIndex of blocking
    BlockedIndices.Emplace();

	auto NumNodes = GetNodesInLayer(1);
	for (auto i = 0; i < NumNodes; i++)
	{
		FVector Location;
		GetNodeLocation(1, i, Location);

		FCollisionQueryParams Params;
		Params.bFindInitialOverlaps = true;
		Params.bTraceComplex = false;
		Params.TraceTag = "SVONFirstPassRasterize";
		if (GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, CollisionChannel, FCollisionShape::MakeBox(FVector(GetVoxelSize(1) * 0.5f)), Params))
			BlockedIndices[0].Add(i);
	}

	int32 LayerIndex = 0;
	while (BlockedIndices[LayerIndex].Num() > 1)
	{
		// Add a new LayerIndex to structure
		BlockedIndices.Emplace();

		// Add any parent morton codes to the new LayerIndex
		for (FMortonCode& Code : BlockedIndices[LayerIndex])
			BlockedIndices[LayerIndex + 1].Add(Code >> 3);

		LayerIndex++;
	}

	return true;
}

bool ASVONVolume::GetNodeLocation(FLayerIndex Layer, FMortonCode Code, FVector& OutLocation) const
{
	auto VoxelSize = GetVoxelSize(Layer);
	uint_fast32_t X, Y, Z;
	morton3D_64_decode(Code, X, Y, Z);

	OutLocation = Origin - Extent + FVector(X * VoxelSize, Y * VoxelSize, Z * VoxelSize) + FVector(VoxelSize * 0.5f);
	
    return true;
}

// Gets the Location of a given link. Returns true if the link is open, false if blocked
bool ASVONVolume::GetLinkLocation(const FSVONLink& Link, FVector& OutLocation) const
{
	const FSVONNode& Node = GetLayer(Link.LayerIndex)[Link.NodeIndex];

	GetNodeLocation(Link.LayerIndex, Node.Code, OutLocation);

	// If this is LayerIndex 0, and there are valid children
	if (Link.LayerIndex == 0 && Node.FirstChild.IsValid())
	{
		auto VoxelSize = GetVoxelSize(0);
		uint_fast32_t X, Y, Z;
		morton3D_64_decode(Link.SubNodeIndex, X,Y,Z);

		OutLocation += FVector(X * VoxelSize * 0.25f, Y * VoxelSize * 0.25f, Z * VoxelSize * 0.25f) - FVector(VoxelSize * 0.375);
		const FSVONLeafNode& LeafNode = GetLeafNode(Node.FirstChild.NodeIndex);
		bool bIsBlocked = LeafNode.GetNode(Link.SubNodeIndex);

		return !bIsBlocked;
	}

	return true;
}

bool ASVONVolume::GetIndexForCode(FLayerIndex LayerIndex, FMortonCode Code, FNodeIndex& OutIndex) const
{
	const TArray<FSVONNode>& Layer = GetLayer(LayerIndex);

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

const FSVONNode& ASVONVolume::GetNode(const FSVONLink& Link) const
{
	if (Link.LayerIndex < 14)
		return GetLayer(Link.LayerIndex)[Link.NodeIndex];
	else
		return GetLayer(NumLayers - 1)[0];
}

const FSVONLeafNode& ASVONVolume::GetLeafNode(FNodeIndex Index) const
{
	return Data.LeafNodes[Index];
}

void ASVONVolume::GetLeafNeighbors(const FSVONLink& Link, TArray<FSVONLink>& OutNeighbors) const
{
    FMortonCode LeafIndex = Link.SubNodeIndex;
    const FSVONNode& Node = GetNode(Link);
	const FSVONLeafNode& Leaf = GetLeafNode(Node.FirstChild.NodeIndex);

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
			const FSVONLink& NeighborLink = Node.Neighbors[i];
			const FSVONNode& NeighborNode = GetNode(NeighborLink);

			// If the Neighbor LayerIndex 0 has no Leaf nodes, just return it
			if (!NeighborNode.FirstChild.IsValid())
			{
				OutNeighbors.Add(NeighborLink);
				continue;
			}

			const FSVONLeafNode& LeafNode = GetLeafNode(NeighborNode.FirstChild.NodeIndex);
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

void ASVONVolume::GetNeighbors(const FSVONLink& Link, TArray<FSVONLink>& OutNeighbors) const
{
	const FSVONNode& Node = GetNode(Link);
	for (auto i = 0; i < 6; i++)
	{
		const FSVONLink& NeighborLink = Node.Neighbors[i];
		if (!NeighborLink.IsValid())
			continue;

		const FSVONNode& Neighbor = GetNode(NeighborLink);

		// If the Neighbor has no children, we just use it
		if (!Neighbor.FirstChild.IsValid())
		{
			OutNeighbors.Add(NeighborLink);
			continue;
		}

		// TODO: This recursive section should be the most accurate, ensuring that when pathfinding down multiple levels (say, 2 to Leaf),
		// That all valid edge nodes (with no children) in that direction are considered
		// Is does mean that the search *explodes* in this scenario.

		//TArray<FSVONLink> workingSet;

		//workingSet.Push(NeighborLink);

		//// Otherwise, we gotta recurse down 

		//while (workingSet.Num() > 0)
		//{
		//	// Pop off the Neighbor
		//	FSVONLink thisLink = workingSet.Pop();

		//	// If it's above LayerIndex 0, we need to add 4 children to explore
		//	if (thisLink.GetLayerIndex() > 0)
		//	{
		//		for (const nodeindex_t& Index : SVONStatics::dirChildOffsets[i])
		//		{
		//			// Each of the childnodes
		//			FSVONLink link = Neighbor.myFirstChild;
		//			link.NodeIndex += Index;
		//			const SVONNode& linkNode = GetNode(link);

		//			if (linkNode.HasChildren()) // If it has children, add them to the list to keep going down
		//			{
		//				workingSet.Emplace(link.GetLayerIndex(), link.GetNodeIndex(), link.GetSubNodeIndex());
		//			}
		//			else // Or just add to the outgoing links
		//			{
		//				OutNeighbors.Add(link);
		//			}
		//		}
		//	}
		//	else
		//	{
		//		for (const nodeindex_t& LeafIndex : SVONStatics::dirLeafChildOffsets[i])
		//		{
		//			// Each of the childnodes
		//			FSVONLink link = Neighbor.myFirstChild;
		//			//link.SubNodeIndex = LeafIndex;
		//			const FSVONLeafNode& LeafNode = GetLeafNode(link.NodeIndex);

		//			if (!LeafNode.GetNode(LeafIndex))
		//			{
		//				OutNeighbors.Add(link);
		//			}
		//		}
		//	}
		//}



		// If the Neighbor has children and is a Leaf Node, we need to add 16 Leaf voxels 
		else if (Neighbor.FirstChild.LayerIndex == 0)
		{
			for (const FNodeIndex& Index : FSVONStatics::DirectionLeafChildOffsets[i])
			{
				// This is the link to our first child, we just need to add our offsets
				auto NeighborLink = Neighbor.FirstChild;
				if(!GetLeafNode(NeighborLink.NodeIndex).GetNode(Index))
					OutNeighbors.Emplace(FSVONLink(NeighborLink.LayerIndex, NeighborLink.NodeIndex, Index));
			}
		}
        // If the Neighbor has children and isn't a Leaf, we just add 4
        //TODO: the problem with this is that you no longer have the direction information to know which subnodes to select,
        //   in the case that *this* child has children
		else
		{
			for (const FNodeIndex& Index : FSVONStatics::DirectionChildOffsets[i])
			{
				// This is the link to our first child, we just need to add our offsets
				auto NeighborLink = Neighbor.FirstChild;
                OutNeighbors.Emplace(FSVONLink(NeighborLink.LayerIndex, NeighborLink.NodeIndex + Index, NeighborLink.SubNodeIndex));
			}
		}
	}
}

float ASVONVolume::GetVoxelSize(FLayerIndex Layer) const
{
	return (Extent.X / FMath::Pow(2, VoxelPower)) * (FMath::Pow(2.0f, Layer + 1));
}

bool ASVONVolume::IsReadyForNavigation()
{
	return bIsReadyForNavigation;
}

int32 ASVONVolume::GetNodesInLayer(FLayerIndex Layer)
{
	return FMath::Pow(FMath::Pow(2, (VoxelPower - (Layer))), 3);
}

int32 ASVONVolume::GetNodesPerSide(FLayerIndex Layer)
{
	return FMath::Pow(2, (VoxelPower - (Layer)));
}

void ASVONVolume::BeginPlay()
{
	if (!bIsReadyForNavigation)
	{
		Generate();
		bIsReadyForNavigation = true;
	}
}

void ASVONVolume::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
}

void ASVONVolume::PostUnregisterAllComponents()
{
	Super::PostUnregisterAllComponents();
}

void ASVONVolume::BuildNeighborLinks(FLayerIndex LayerIndex)
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
				&& LayerIndex < Data.Layer.Num() - 2)
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

bool ASVONVolume::FindLinkInDirection(FLayerIndex LayerIndex, const FNodeIndex NodeIndex, uint8 Direction, FSVONLink& OutLinkToUpdate, FVector& OutStartLocation)
{
	auto MaxCoord = GetNodesPerSide(LayerIndex);
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
		if (bShowNeighborLinks)
		{
			FVector StartLocation, EndLocation;
			GetNodeLocation(LayerIndex, Node.Code, StartLocation);
			EndLocation = StartLocation + (FVector(FSVONStatics::Directions[Direction]) * 100.f);
			DrawDebugLine(GetWorld(), OutStartLocation, EndLocation, FColor::Red, true, -1.f, 0, .0f);
		}

		return true;
	}

	X = SX; Y = SY; Z = SZ;

	// Get the morton Code for the direction
	auto Code = morton3D_64_encode(X, Y, Z);
	bool bIsHigher = Code > Node.Code;
	auto NodeDelta = (bIsHigher ?  1 : - 1);

	while ((NodeIndex + NodeDelta) < Layer.Num() && NodeIndex + NodeDelta >= 0)
	{
		// This is the Node we're looking for
        if (Layer[NodeIndex + NodeDelta].Code == Code)
        {
            const FSVONNode& Node = Layer[NodeIndex + NodeDelta];
            // This is a Leaf Node
            if (LayerIndex == 0 && Node.HasChildren())
            {
                // Set invalid link if the Leaf Node is completely blocked, no point linking to it
                if (GetLeafNode(Node.FirstChild.NodeIndex).IsCompletelyBlocked())
                {
                    OutLinkToUpdate.SetInvalid();
                    return true;
                }
            }
            // Otherwise, use this link
            OutLinkToUpdate.LayerIndex = LayerIndex;
            check(NodeIndex + NodeDelta < Layer.Num());
            OutLinkToUpdate.NodeIndex = NodeIndex + NodeDelta;

            if (bShowNeighborLinks)
            {
                FVector EndLocation;
                GetNodeLocation(LayerIndex, Code, EndLocation);
                DrawDebugLine(GetWorld(), OutStartLocation, EndLocation, FSVONStatics::LinkColors[LayerIndex], true, -1.f, 0, .0f);
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

void ASVONVolume::RasterizeLeafNode(FVector& Origin, FNodeIndex LeafIndex)
{
	for (auto i = 0; i < 64; i++)
	{
		uint_fast32_t X, Y, Z;
		morton3D_64_decode(i, X, Y, Z);

		float LeafVoxelSize = GetVoxelSize(0) * 0.25f;
		FVector Location = Origin + FVector(X * LeafVoxelSize, Y * LeafVoxelSize, Z * LeafVoxelSize) + FVector(LeafVoxelSize * 0.5f);

		if (LeafIndex >= Data.LeafNodes.Num() - 1)
			Data.LeafNodes.AddDefaulted(1);

		if(IsBlocked(Location, LeafVoxelSize * 0.5f))
		{
			Data.LeafNodes[LeafIndex].SetNode(i);

			if (bShowLeafVoxels)
				DrawDebugBox(GetWorld(), Location, FVector(LeafVoxelSize * 0.5f), FQuat::Identity, FColor::Red, true, -1.f, 0, .0f);
		}
	}
}

TArray<FSVONNode>& ASVONVolume::GetLayer(FLayerIndex LayerIndex)
{
	return Data.Layer[LayerIndex];
}

const TArray<FSVONNode>& ASVONVolume::GetLayer(FLayerIndex LayerIndex) const
{
	return Data.Layer[LayerIndex];
}

// Check for blocking...using this cached set for each LayerIndex for now for fast lookups
bool ASVONVolume::IsAnyMemberBlocked(FLayerIndex LayerIndex, FMortonCode Code)
{
	FMortonCode ParentCode = Code >> 3;
	if (LayerIndex == BlockedIndices.Num())
		return true;

	// The parent of this Code is blocked
	if (BlockedIndices[LayerIndex].Contains(ParentCode))
		return true;

	return false;
}

bool ASVONVolume::IsBlocked(const FVector& Location, const float Size) const
{
	FCollisionQueryParams Params;
	Params.bFindInitialOverlaps = true;
	Params.bTraceComplex = false;
	Params.TraceTag = "SVONLeafRasterize";

	return GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, CollisionChannel, FCollisionShape::MakeBox(FVector(Size + Clearance)), Params);
}

bool ASVONVolume::SetNeighbor(const FLayerIndex LayerIndex, const FNodeIndex ArrayIndex, const EDirection Direction)
{
	return false;
}

void ASVONVolume::RasterizeLayer(FLayerIndex LayerIndex)
{
    FNodeIndex LeafIndex = 0;
    // LayerIndex 0 Leaf nodes are special
    if (LayerIndex == 0)
    {
        // Run through all our coordinates
        auto NumNodes = GetNodesInLayer(LayerIndex);
        for (auto i = 0; i < NumNodes; i++)
        {
            int Index = i;

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
                if (bShowMortonCodes)
                    DrawDebugString(GetWorld(), NodeLocation, FString::FromInt(Node.Code), nullptr, FSVONStatics::LayerColors[LayerIndex], -1, false);

                if (bShowVoxels)
                    DrawDebugBox(GetWorld(), NodeLocation, FVector(GetVoxelSize(LayerIndex) * 0.5f), FQuat::Identity, FSVONStatics::LayerColors[LayerIndex], true, -1.f, 0, .0f);

                // Now check if we have any blocking, and search Leaf nodes
                FVector Location;
                GetNodeLocation(0, i, Location);

                FCollisionQueryParams Params;
                Params.bFindInitialOverlaps = true;
                Params.bTraceComplex = false;
                Params.TraceTag = "SVONRasterize";

                if (IsBlocked(Location, GetVoxelSize(0) * 0.5f))
                {
                    // Rasterize my Leaf nodes
                    FVector LeafOrigin = NodeLocation - (FVector(GetVoxelSize(LayerIndex) * 0.5f));
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
        int32 NodeCounter = 0;
        int32 NumNodes = GetNodesInLayer(LayerIndex);
        for (auto i = 0; i < NumNodes; i++)
        {
            // Do we have any blocking children, or siblings?
            // Remember we must have 8 children per parent
            if (IsAnyMemberBlocked(LayerIndex, i))
            {
                // Add a Node
                auto Index = GetLayer(LayerIndex).Emplace();
                NodeCounter++;
                FSVONNode& Node = GetLayer(LayerIndex)[Index];

                // Set details
                Node.Code = i;
                FNodeIndex ChildIndex = 0;
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
                    if (bShowVoxels)
                        DrawDebugBox(GetWorld(), NodeLocation, FVector(GetVoxelSize(LayerIndex) * 0.5f), FQuat::Identity, FSVONStatics::LayerColors[LayerIndex], true, -1.f, 0, .0f);

                    if (bShowMortonCodes)
                        DrawDebugString(GetWorld(), NodeLocation, FString::FromInt(Node.Code), nullptr, FSVONStatics::LayerColors[LayerIndex], -1, false);
                }
            }
        }
    }
}