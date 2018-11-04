#pragma once

class ASVONVolume;
struct FSVONLink;

class UESVON_API FSVONMediator
{
public:
	static bool GetLinkFromLocation(const FVector& Location, const ASVONVolume& Volume, FSVONLink& oLink);
	static void GetVolumeXYZ(const FVector& Location, const ASVONVolume& Volume, const int Layer, FIntVector& OutLocation);
};