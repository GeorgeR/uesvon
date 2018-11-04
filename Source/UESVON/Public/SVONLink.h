#pragma once

#include "CoreMinimal.h"

struct UESVON_API FSVONLink
{
public:
    uint8 LayerIndex:4;
    uint_fast32_t NodeIndex:22;
	uint32 SubNodeIndex:6;

	FSVONLink() 
        : LayerIndex(15),
		NodeIndex(0),
		SubNodeIndex(0) { }

	FSVONLink(uint8 Layer, uint_fast32_t NodeIndex, uint8 SubNodeIndex)
		: LayerIndex(Layer),
		NodeIndex(NodeIndex),
		SubNodeIndex(SubNodeIndex) {}

	bool IsValid() const { return LayerIndex != 15; }
	void SetInvalid() { LayerIndex = 15; }

	bool operator==(const FSVONLink& Other) const { return memcmp(this, &Other, sizeof(FSVONLink)) == 0; }

	static FSVONLink GetInvalidLink() { return FSVONLink(15, 0, 0); }

	FString ToString() { return FString::Printf(TEXT("%i:%i:%i"), LayerIndex, NodeIndex, SubNodeIndex);	}
};

FORCEINLINE uint32 GetTypeHash(const FSVONLink& Value)
{
	return FCrc::MemCrc_DEPRECATED(&Value, sizeof(FSVONLink));
}