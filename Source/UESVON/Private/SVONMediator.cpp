#include "SVONMediator.h"
#include "CoreMinimal.h"
#include "SVONVolume.h"
#include "SVONLink.h"
#include "DrawDebugHelpers.h"

bool FSVONMediator::GetLinkFromLocation(const FVector& Location, const ASVONVolume& Volume, FSVONLink& OutLink)
{
	// Location is outside the volume, no can do
	if (!Volume.EncompassesPoint(Location))
		return false;

	auto Box = Volume.GetComponentsBoundingBox(true);

	FVector Origin;
	FVector Extent;

	Box.GetCenterAndExtents(Origin, Extent);

	// The Z-order Origin of the volume (where Code == 0)
	auto OriginZ = Origin - Extent;

	// The local Location of the point in volume space
	auto LocalLocation = Location - OriginZ;

	auto LayerIndex = Volume.GetNumLayers() - 1;
	FNodeIndex NodeIndex = 0;
	while (LayerIndex >= 0 && LayerIndex < Volume.GetNumLayers())
	{
		// Get the Layer and Voxel size

		const TArray<FSVONNode>& Layer = Volume.GetLayer(LayerIndex);
		// Calculate the XYZ coordinates

		FIntVector Voxel;
		GetVolumeXYZ(Location, Volume, LayerIndex, Voxel);

		uint_fast32_t X, Y, Z;
		X = Voxel.X;
		Y = Voxel.Y;
		Z = Voxel.Z;

		// Get the morton Code we want for this Layer
		FMortonCode Code = morton3D_64_encode(X, Y, Z);

		for (FNodeIndex j = NodeIndex; j < Layer.Num(); j++)
		{
			const FSVONNode& Node = Layer[j];

			// This is the Node we are in
			if (Node.Code == Code)
			{
				// There are no child nodes, so this is our nav Location
				if (!Node.FirstChild.IsValid())// && LayerIndex > 0)
				{
                    OutLink.LayerIndex = LayerIndex;
                    OutLink.NodeIndex = j;
                    OutLink.SubNodeIndex = 0;
					
					return true;
				}

				// If this is a Leaf Node, we need to find our subnode
				if (LayerIndex == 0)
				{
					const FSVONLeafNode& Leaf = Volume.GetLeafNode(Node.FirstChild.NodeIndex);

					// We need to calculate the Node local Location to get the morton Code for the Leaf
					float VoxelSize = Volume.GetVoxelSize(LayerIndex);

					// The world Location of the 0 Node
					FVector NodeLocation;
					Volume.GetNodeLocation(LayerIndex, Node.Code, NodeLocation);

					// The morton Origin of the Node
					auto NodeOrigin = NodeLocation - FVector(VoxelSize * 0.5f);

					// The requested Location, relative to the Node Origin
					auto NodeLocalLocation = Location - NodeOrigin;

					// Now get our Voxel coordinates
					FIntVector Coordinate;
					Coordinate.X = FMath::FloorToInt((NodeLocalLocation.X / (VoxelSize * 0.25f)));
					Coordinate.Y = FMath::FloorToInt((NodeLocalLocation.Y / (VoxelSize * 0.25f)));
					Coordinate.Z = FMath::FloorToInt((NodeLocalLocation.Z / (VoxelSize * 0.25f)));

					// So our link is.....*drum roll*
					OutLink.LayerIndex = 0; // Layer 0 (Leaf)
					OutLink.NodeIndex = j; // This index

					FMortonCode LeafIndex = morton3D_64_encode(Coordinate.X, Coordinate.Y, Coordinate.Z); // This morton Code is our key into the 64-bit Leaf Node

					if (Leaf.GetNode(LeafIndex))
						return false;// This Voxel is blocked, oops!

					OutLink.SubNodeIndex = LeafIndex;

					return true;
				}
				
				// If we've got here, the current Node has a child, and isn't a Leaf, so lets go down...
				LayerIndex = Layer[j].FirstChild.GetLayerIndex();
				NodeIndex = Layer[j].FirstChild.GetNodeIndex();

				break; //stop iterating this Layer
			}
		}
	}

	return false;
}

void FSVONMediator::GetVolumeXYZ(const FVector& Location, const ASVONVolume& Volume, const int Layer, FIntVector& OutXYZ)
{
	auto Box = Volume.GetComponentsBoundingBox(true);

	FVector Origin;
	FVector Extent;

	Box.GetCenterAndExtents(Origin, Extent);

	// The Z-order Origin of the volume (where Code == 0)
	auto OriginZ = Origin - Extent;

	// The local Location of the point in volume space
	auto LocalLocation = Location - OriginZ;

	auto LayerIndex = Layer;

	// Get the Layer and Voxel size
	auto VoxelSize = Volume.GetVoxelSize(LayerIndex);
	
	// Calculate the XYZ coordinates
	OutXYZ.X = FMath::FloorToInt((LocalLocation.X / VoxelSize));
	OutXYZ.Y = FMath::FloorToInt((LocalLocation.Y / VoxelSize));
	OutXYZ.Z = FMath::FloorToInt((LocalLocation.Z / VoxelSize));
}