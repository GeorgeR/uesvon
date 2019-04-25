#pragma once

#include "CoreMinimal.h"

struct UESVON_API FSVONLink
{
private:
	typedef uint8 FLayerIndex;
	typedef uint32 FNodeIndex;
	typedef uint8 FSubNodeIndex;

	static const FLayerIndex InvalidLayerIndex = 15;

public:
	FLayerIndex LayerIndex:4;
	FNodeIndex NodeIndex:22;
	FSubNodeIndex SubNodeIndex:6;

	FSVONLink() 
        : LayerIndex(15),
		NodeIndex(0),
		SubNodeIndex(0) { }

	FSVONLink(FLayerIndex Layer, FNodeIndex NodeIndex, FSubNodeIndex SubNodeIndex)
		: LayerIndex(Layer),
		NodeIndex(NodeIndex),
		SubNodeIndex(SubNodeIndex) {}

	FLayerIndex GetLayerIndex() const { return LayerIndex; }
	void SetLayerIndex(const FLayerIndex Value) { this->LayerIndex = Value; }

	FNodeIndex GetNodeIndex() const { return NodeIndex; }
	void SetNodeIndex(const FNodeIndex Value) { this->NodeIndex = Value; }

	FSubNodeIndex GetSubNodeIndex() const { return SubNodeIndex; }
	void SetSubNodeIndex(const FSubNodeIndex Value) { this->SubNodeIndex = Value; }

	bool IsValid() const { return LayerIndex != InvalidLayerIndex; }
	void SetInvalid() { LayerIndex = InvalidLayerIndex; }

	bool operator==(const FSVONLink& Other) const { return FMemory::Memcmp(this, &Other, sizeof(FSVONLink)) == 0; }

	static FSVONLink GetInvalidLink() { return FSVONLink(InvalidLayerIndex, 0, 0); }

	FString ToString() { return FString::Printf(TEXT("%i:%i:%i"), LayerIndex, NodeIndex, SubNodeIndex);	}
};

FORCEINLINE uint32 GetTypeHash(const FSVONLink& Value)
{
	return FCrc::MemCrc32(&Value, sizeof(FSVONLink));
}

FORCEINLINE FArchive& operator<<(FArchive& Ar, FSVONLink& Link)
{
	Ar.Serialize(&Link, sizeof(FSVONLink));
	return Ar;
}
