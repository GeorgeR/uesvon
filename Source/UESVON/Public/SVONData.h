#pragma once

#include "CoreMinimal.h"

#include "SVONNode.h"
#include "SVONLeafNode.h"

struct FSVONData
{
public:
	TArray<TArray<FSVONNode>> Layer;
	TArray<FSVONLeafNode> LeafNodes;
};