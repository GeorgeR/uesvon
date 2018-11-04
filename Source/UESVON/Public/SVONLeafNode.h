#pragma once

#include "libmorton/morton.h"

struct UESVON_API FSVONLeafNode
{
public:
	uint_fast64_t VoxelGrid = 0;

	inline bool GetNodeAt(uint_fast32_t X, uint_fast32_t Y, uint_fast32_t Z) const
	{
		uint_fast64_t index = 0;
		morton3D_64_decode(index, X, Y, Z);
		return (VoxelGrid & (1ULL << index)) != 0;
	}

	inline void SetNodeAt(uint_fast32_t X, uint_fast32_t Y, uint_fast32_t Z)
	{
		uint_fast64_t index = 0;
		morton3D_64_decode(index, X, Y, Z);
		VoxelGrid |= 1ULL << index;
	}

	inline void SetNode(uint8 Index)
	{
		VoxelGrid |= 1ULL << Index;
	}

	inline bool GetNode(FMortonCode Index) const
	{
		return (VoxelGrid & (1ULL << Index)) != 0;
	}

	inline bool IsCompletelyBlocked() const
	{
		return VoxelGrid == -1;
	}

	inline bool IsEmpty() const
	{
		return VoxelGrid == 0;
	}
};