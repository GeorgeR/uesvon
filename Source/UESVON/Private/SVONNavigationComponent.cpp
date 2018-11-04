#include "SVONNavigationComponent.h"

#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "SVONVolume.h"
#include "SVONLink.h"
#include "SVONPathFinder.h"
#include "SVONPath.h"
#include "SVONFindPathTask.h"
#include "DrawDebugHelpers.h"
#include "NavigationData.h"
#include "Runtime/Engine/Classes/Components/LineBatchComponent.h "

// Sets default values for this component's properties
USVONNavigationComponent::USVONNavigationComponent()
	: bIsBusy(false)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	LastLocation = FSVONLink(0, 0, 0);
}

// Called when the game starts
void USVONNavigationComponent::BeginPlay()
{
	Super::BeginPlay();
}

/** Are we inside a valid nav volume ? */
bool USVONNavigationComponent::HasNavVolume()
{
	return CurrentNavVolume && GetOwner() && CurrentNavVolume->EncompassesPoint(GetOwner()->GetActorLocation());
}

bool USVONNavigationComponent::FindVolume()
{
	TArray<AActor*> NavVolumes;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASVONVolume::StaticClass(), NavVolumes);

	for (AActor* Actor : NavVolumes)
	{
		auto Volume = Cast<ASVONVolume>(Actor);
		if (Volume && Volume->EncompassesPoint(GetOwner()->GetActorLocation()))
		{
			CurrentNavVolume = Volume;
			return true;
		}
	}
	return false;
}

// Called every frame
void USVONNavigationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	//return;

	if (!HasNavVolume())
		FindVolume();
	else if (CurrentNavVolume->IsReadyForNavigation() && !bIsBusy)
	{
		FVector Location = GetOwner()->GetActorLocation();
		if (DebugPrintMortonCodes)
			DebugLocalLocation(Location);

		auto Link = GetNavLocation(Location);
	}

	int32 JobIndex;
	if (JobQueue.Dequeue(JobIndex))
	{
		//GetWorld()->PersistentLineBatcher->Flush();
		if (JobIndex > 0)
			PointDebugIndex = 0;
		else
			bIsBusy = false;
	}

	if (bIsBusy && PointDebugIndex > -1)
	{
		if (bDebugDrawOpenNodes)
		{
			if (DebugPoints.Num() > 0)
			{
                DrawDebugSphere(GetWorld(), DebugPoints[PointDebugIndex], 100.f, 5, FColor::Red, true);
                DrawDebugString(GetWorld(), DebugPoints[PointDebugIndex], FString::FromInt(PointDebugIndex));
			}

			if (PointDebugIndex < DebugPoints.Num() - 1)
				PointDebugIndex++;
			else
			{
				bIsBusy = false;
				PointDebugIndex = -1;
			}
		}
		else
		{
			bIsBusy = false;
			PointDebugIndex = -1;
		}
	}
}

FSVONLink USVONNavigationComponent::GetNavLocation(FVector& OutLocation)
{
	FSVONLink NavLink;
	if (HasNavVolume())
	{
		// Get the nav link from our volume
		FSVONMediator::GetLinkFromLocation(GetOwner()->GetActorLocation(), *CurrentNavVolume, NavLink);

		if (NavLink == LastLocation)
			return NavLink;

		LastLocation = NavLink;

		auto TargetLocation = GetOwner()->GetActorLocation() + (GetOwner()->GetActorForwardVector() * 10000.f);

		if (bDebugPrintCurrentPosition)
		{
			const FSVONNode& CurrentNode = CurrentNavVolume->GetNode(NavLink);
			FVector CurrentNodePosition;

			bool bIsValid = CurrentNavVolume->GetLinkLocation(NavLink, CurrentNodePosition);

			DrawDebugLine(GetWorld(), GetOwner()->GetActorLocation(), CurrentNodePosition, isValid ? FColor::Green : FColor::Red, false, -1.f, 0, 10.f);
			DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + FVector(0.f, 0.f, -50.f), NavLink.ToString(), nullptr, FColor::Yellow, 0.01f);
		}	
	}

	return NavLink;
}

bool USVONNavigationComponent::FindPathAsync(const FVector& StartLocation, const FVector& TargetLocation, FNavPathSharedPtr* OutNavPath)
{
	UE_LOG(UESVON, Display, TEXT("Finding path from %s and %s"), *GetOwner()->GetActorLocation().ToString(), *TargetLocation.ToString());

	FSVONLink StartNavLink;
	FSVONLink TargetNavLink;

	if (HasNavVolume())
	{
		// Get the nav link from our volume
		if (!FSVONMediator::GetLinkFromLocation(GetOwner()->GetActorLocation(), *CurrentNavVolume, StartNavLink))
		{
			UE_LOG(UESVON, Display, TEXT("Path finder failed to find start nav link"));
			return false;
		}

		if (!FSVONMediator::GetLinkFromLocation(aTargetPosition, *CurrentNavVolume, targetNavLink))
		{
			UE_LOG(UESVON, Display, TEXT("Path finder failed to find target nav link"));
			return false;
		}

		DebugPoints.Empty();
		PointDebugIndex = -1;

		(new FAutoDeleteAsyncTask<FSVONFindPathTask>(*CurrentNavVolume, GetWorld(), StartNavLink, TargetNavLink, StartLocation, TargetLocation, NavPath, JobQueue, DebugPoints))->StartBackgroundTask();

		bIsBusy = true;

        return true;
	}

	return false;
}

bool USVONNavigationComponent::FindPathImmediate(const FVector& StartLocation, const FVector& StartLocation, FNavPathSharedPtr* OutNavPath)
{
	UE_LOG(UESVON, Display, TEXT("Finding path immediate from %s and %s"), *StartLocation.ToString(), *TargetLocation.ToString());

	FSVONLink StartNavLink;
	FSVONLink TargetNavLink;
	if (HasNavVolume())
	{
		// Get the nav link from our volume
		if (!FSVONMediator::GetLinkFromLocation(StartLocation, *CurrentNavVolume, StartNavLink))
		{
			UE_LOG(UESVON, Display, TEXT("Path finder failed to find start nav link"));
			return false;
		}

		if (!FSVONMediator::GetLinkFromLocation(TargetLocation, *CurrentNavVolume, TargetNavLink))
		{
			UE_LOG(UESVON, Display, TEXT("Path finder failed to find target nav link"));
			return false;
		}

		if (!OutNavPath || !OutNavPath->IsValid())
		{
			UE_LOG(UESVON, Display, TEXT("Nav path data invalid"));
			return false;
		}

		auto Path = OutNavPath->Get();
		Path->ResetForRepath();

		DebugPoints.Empty();
		PointDebugIndex = -1;

		TArray<FVector> DebugOpenPoints;

		FSVONPathFinderSettings Settings;
		Settings.bUseUnitCost = UseUnitCost;
		Settings.UnitCost = UnitCost;
		Settings.EstimateWeight = EstimateWeight;
		Settings.NodeSizeCompensation = NodeSizeCompensation;
		Settings.PathCostType = PathCostType;
		Settings.SmoothingIterations = SmoothingIterations;

		FSVONPathFinder PathFinder(GetWorld(), *CurrentNavVolume, Settings);

		auto Result = PathFinder.FindPath(StartNavLink, TargetNavLink, StartLocation, TargetLocation, OutNavPath);

		bIsBusy = true;
		PointDebugIndex = 0;

		Path->MarkReady();

		return true;
	}

	return false;
}

void USVONNavigationComponent::DebugLocalLocation(FVector& OutLocation) 
{
	if (HasNavVolume())
	{
		for (int i = 0; i < CurrentNavVolume->GetNumLayers() - 1; i++)
		{
			FIntVector Location;
			FSVONMediator::GetVolumeXYZ(GetOwner()->GetActorLocation(), *CurrentNavVolume, i, Location);

			auto Code = morton3D_64_encode(Location.X, Location.Y, Location.Z);
			auto CodeString = FString::FromInt(code);
			DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + FVector(0.f, 0.f, i * 50.0f), Location.ToString() + " - " + CodeString, nullptr, FColor::White, 0.01f);
		}
	}
}