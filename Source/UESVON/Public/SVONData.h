#pragma once

#include "CoreMinimal.h"

#include "SVONNode.h"
#include "SVONLeafNode.h"

struct FSVONData
{
public:
	TArray<TArray<FSVONNode>> Layers;
	TArray<FSVONLeafNode> LeafNodes;

	void Reset()
	{
		Layers.Empty();
		LeafNodes.Empty();
	}

	int32 GetSize()
	{
		auto Result = 0;
		Result += LeafNodes.Num() * sizeof(FSVONLeafNode);
		for (auto i = 0; i < Layers.Num(); i++)
			Result += Layers[i].Num() * sizeof(FSVONNode);
		return Result;
	}
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FSVONData& Data)
{
	Ar << Data.Layers;
	Ar << Data.LeafNodes;
	return Ar;
}
