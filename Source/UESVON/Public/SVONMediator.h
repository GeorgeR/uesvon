#pragma once

class ASVONVolumeActor;
struct FSVONLink;

class UESVON_API FSVONMediator
{
public:
	static bool GetLinkFromLocation(const FVector& Location, const ASVONVolumeActor& Volume, FSVONLink& oLink);
	static void GetVolumeXYZ(const FVector& Location, const ASVONVolumeActor& Volume, const int Layer, FIntVector& OutLocation);
};
