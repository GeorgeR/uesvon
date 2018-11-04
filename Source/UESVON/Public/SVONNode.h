#pragma once

#include "CoreMinimal.h"

#include "SVONLink.h"
#include "SVONDefines.h"

struct UESVON_API FSVONNode
{
public:
	FMortonCode Code;

	FSVONLink Parent;
	FSVONLink FirstChild;

	FSVONLink Neighbors[6];

	FSVONNode() 
        : Parent(FSVONLink::GetInvalidLink()),
        FirstChild(FSVONLink::GetInvalidLink()) { }

	FORCEINLINE bool HasChildren() const { return FirstChild.IsValid(); }
};