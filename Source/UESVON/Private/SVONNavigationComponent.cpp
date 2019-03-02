#include "SVONNavigationComponent.h"

#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "NavigationData.h"
#include "Components/LineBatchComponent.h "

#include "SVONVolumeActor.h"
#include "SVONLink.h"
#include "SVONPathFinder.h"
#include "SVONNavigationPath.h"
#include "SVONFindPathTask.h"
#include "SVONMediator.h"

// Sets default values for this component's properties
USVONNavigationComponent::USVONNavigationComponent()
	: bIsBusy(false)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	LastLocation = FSVONLink(0, 0, 0);

	SVONPath = MakeShareable<FSVONNavigationPath>(new FSVONNavigationPath());
}

// Called when the game starts
void USVONNavigationComponent::BeginPlay()
{
	Super::BeginPlay();
}

/** Are we inside a valid nav volume ? */
bool USVONNavigationComponent::HasNavVolume()
{
	return CurrentNavVolume
		&& GetOwner()
		&& CurrentNavVolume->EncompassesPoint(GetPawnLocation())
		&& CurrentNavVolume->GetNumLayers() > 0;
}

bool USVONNavigationComponent::FindVolume()
{
	TArray<AActor*> NavVolumes;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASVONVolumeActor::StaticClass(), NavVolumes);

	for (AActor* Actor : NavVolumes)
	{
		auto Volume = Cast<ASVONVolumeActor>(Actor);
		if (Volume && Volume->EncompassesPoint(GetPawnLocation()))
		{
			CurrentNavVolume = Volume;
			return true;
		}
	}

	return false;
}

FVector USVONNavigationComponent::GetPawnLocation()
{
	FVector Result;
	auto Controller = Cast<AController>(GetOwner());
	if(Controller)
		if(auto Pawn = Controller->GetPawn())
			Result = Pawn->GetActorLocation();

	return Result;
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
		if (bDebugPrintMortonCodes)
			DebugLocalLocation(Location);

		auto Link = GetNavLocation(Location);
	}

	int32 JobIndex;
	if (JobQueue.Dequeue(JobIndex))
	{
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

		//auto TargetLocation = GetPawnLocation() + (GetOwner()->GetActorForwardVector() * 10000.f);

		if (bDebugPrintCurrentPosition)
		{
			const FSVONNode& CurrentNode = CurrentNavVolume->GetNode(NavLink);
			FVector CurrentNodePosition;

			bool bIsValid = CurrentNavVolume->GetLinkLocation(NavLink, CurrentNodePosition);

			DrawDebugLine(GetWorld(), GetPawnLocation(), CurrentNodePosition, bIsValid ? FColor::Green : FColor::Red, false, -1.f, 0, 10.f);
			DrawDebugString(GetWorld(), GetPawnLocation() + FVector(0.f, 0.f, -50.f), NavLink.ToString(), nullptr, FColor::Yellow, 0.01f);
		}	
	}

	return NavLink;
}

bool USVONNavigationComponent::FindPathAsync(const FVector& StartLocation, const FVector& TargetLocation, FThreadSafeBool& CompleteFlag, FSVONNavPathSharedPtr* OutNavPath)
{
#if WITH_EDITOR
	UE_LOG(UESVON, Display, TEXT("Finding path from %s and %s"), *StartLocation.ToString(), *TargetLocation.ToString());
#endif

	FSVONLink StartNavLink;
	FSVONLink TargetNavLink;

	if (HasNavVolume())
	{
		// Get the nav link from our volume
		if (!FSVONMediator::GetLinkFromLocation(StartLocation, *CurrentNavVolume, StartNavLink))
		{
#if WITH_EDITOR
			UE_LOG(UESVON, Display, TEXT("Path finder failed to find start nav link"));
#endif
			return false;
		}

		if (!FSVONMediator::GetLinkFromLocation(TargetLocation, *CurrentNavVolume, TargetNavLink))
		{
#if WITH_EDITOR
			UE_LOG(UESVON, Display, TEXT("Path finder failed to find target nav link"));
#endif
			return false;
		}

		DebugPoints.Empty();
		PointDebugIndex = -1;

		FSVONPathFinderSettings Settings;
		Settings.bUseUnitCost = bUseUnitCost;
		Settings.UnitCost = UnitCost;
		Settings.WeightEstimate = WeightEstimate;
		Settings.NodeSizeCompensation = NodeSizeCompensation;
		Settings.PathCostType = PathCostType;
		Settings.SmoothingIterations = SmoothingIterations;

		(new FAutoDeleteAsyncTask<FSVONFindPathTask>(*CurrentNavVolume, Settings, GetWorld(), StartNavLink, TargetNavLink, StartLocation, TargetLocation, OutNavPath, CompleteFlag, DebugPoints))->StartBackgroundTask();

		bIsBusy = true;

        return true;
	}

	return false;
}

bool USVONNavigationComponent::FindPathImmediate(const FVector& StartLocation, const FVector& TargetLocation, FSVONNavPathSharedPtr* OutNavPath)
{
#if WITH_EDITOR
	UE_LOG(UESVON, Display, TEXT("Finding path immediate from %s and %s"), *StartLocation.ToString(), *TargetLocation.ToString());
#endif

	FSVONLink StartNavLink;
	FSVONLink TargetNavLink;
	if (HasNavVolume())
	{
		// Get the nav link from our volume
		if (!FSVONMediator::GetLinkFromLocation(StartLocation, *CurrentNavVolume, StartNavLink))
		{
#if WITH_EDITOR
			UE_LOG(UESVON, Display, TEXT("Path finder failed to find start nav link"));
#endif
			return false;
		}

		if (!FSVONMediator::GetLinkFromLocation(TargetLocation, *CurrentNavVolume, TargetNavLink))
		{
#if WITH_EDITOR
			UE_LOG(UESVON, Display, TEXT("Path finder failed to find target nav link"));
#endif
			return false;
		}

		if (!OutNavPath || !OutNavPath->IsValid())
		{
#if WITH_EDITOR
			UE_LOG(UESVON, Display, TEXT("Nav path data invalid"));
#endif
			return false;
		}

		auto Path = OutNavPath->Get();
		Path->ResetForRepath();

		DebugPoints.Empty();
		PointDebugIndex = -1;

		TArray<FVector> DebugOpenPoints;

		FSVONPathFinderSettings Settings;
		Settings.bUseUnitCost = bUseUnitCost;
		Settings.UnitCost = UnitCost;
		Settings.WeightEstimate = WeightEstimate;
		Settings.NodeSizeCompensation = NodeSizeCompensation;
		Settings.PathCostType = PathCostType;
		Settings.SmoothingIterations = SmoothingIterations;

		FSVONPathFinder PathFinder(GetWorld(), *CurrentNavVolume, Settings);

		auto Result = PathFinder.FindPath(StartNavLink, TargetNavLink, StartLocation, TargetLocation, OutNavPath);

		bIsBusy = true;
		PointDebugIndex = 0;

		Path->SetIsReady(true);

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
			FSVONMediator::GetVolumeXYZ(GetPawnLocation(), *CurrentNavVolume, i, Location);

			auto Code = morton3D_64_encode(Location.X, Location.Y, Location.Z);
			auto CodeString = FString::FromInt(Code);
			DrawDebugString(GetWorld(), GetPawnLocation() + FVector(0.f, 0.f, i * 50.0f), Location.ToString() + " - " + CodeString, nullptr, FColor::White, 0.01f);
		}
	}
}
