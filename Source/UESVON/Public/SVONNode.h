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

FORCEINLINE FArchive& operator<<(FArchive& Ar, FSVONNode& Node)
{
	Ar << Node.Code;
	Ar << Node.Parent;
	Ar << Node.FirstChild;

	for (auto i = 0; i < 6; i++)
		Ar << Node.Neighbors[i];

	return Ar;
}
