#include "AITask_SVONMoveTo.h"

#include "UObject/Package.h"
#include "TimerManager.h"
#include "AISystem.h"
#include "AIController.h"
#include "VisualLogger/VisualLogger.h"
#include "AIResources.h"
#include "GameplayTasksComponent.h"
#include "GameplayTask.h"

#include "UESVON.h"
#include "SVONNavigationComponent.h"
#include "SVONVolumeActor.h"

UAITask_SVONMoveTo::UAITask_SVONMoveTo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsPausable = true;
	MoveRequestID = FAIRequestID::InvalidRequest;

	MoveRequest.SetAcceptanceRadius(GET_AI_CONFIG_VAR(AcceptanceRadius));
	MoveRequest.SetReachTestIncludesAgentRadius(GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap));
	MoveRequest.SetAllowPartialPath(GET_AI_CONFIG_VAR(bAcceptPartialPaths));
	MoveRequest.SetUsePathfinding(true);

	Result.Code = ESVONPathfindingRequestResult::SPRR_Failed;

	AddRequiredResource(UAIResource_Movement::StaticClass());
	AddClaimedResource(UAIResource_Movement::StaticClass());

	MoveResult = EPathFollowingResult::Invalid;
	bUseContinuousTracking = false;

	Path = MakeShareable<FNavigationPath>(new FNavigationPath());
}

UAITask_SVONMoveTo* UAITask_SVONMoveTo::SVONAIMoveTo(AAIController* Controller, FVector GoalLocation, bool bUseAsyncPathFinding, AActor* GoalActor,
	float AcceptanceRadius, EAIOptionFlag::Type StopOnOverlap, bool bLockAILogic, bool bUseContinuousGoalTracking)
{
	auto Task = Controller ? UAITask::NewAITask<UAITask_SVONMoveTo>(*Controller, EAITaskPriority::High) : nullptr;
	if (Task)
	{
		Task->bUseAsyncPathFinding = bUseAsyncPathFinding;
		Task->bTickingTask = bUseAsyncPathFinding;

		FAIMoveRequest MoveRequest;
		if (GoalActor)
			MoveRequest.SetGoalActor(GoalActor);
		else
			MoveRequest.SetGoalLocation(GoalLocation);

		MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
		MoveRequest.SetReachTestIncludesAgentRadius(FAISystem::PickAIOption(StopOnOverlap, MoveRequest.IsReachTestIncludingAgentRadius()));
		if (Controller)
			MoveRequest.SetNavigationFilter(Controller->GetDefaultNavigationFilterClass());

		Task->SetUp(Controller, MoveRequest, bUseAsyncPathFinding);
		Task->SetContinuousGoalTracking(bUseContinuousGoalTracking);

		if (bLockAILogic)
			Task->RequestAILogicLocking();
	}

	return Task;
}

void UAITask_SVONMoveTo::SetUp(AAIController* Controller, const FAIMoveRequest& MoveRequest, bool bUseAsyncPathFinding)
{
	this->OwnerController = Controller;
	this->MoveRequest = MoveRequest;
	this->bUseAsyncPathFinding = bUseAsyncPathFinding;
	bTickingTask = bUseAsyncPathFinding;

	// Fail if the owner doesn't have the Navigation Component
	NavigationComponent = Cast<USVONNavigationComponent>(GetOwnerActor()->GetComponentByClass(USVONNavigationComponent::StaticClass()));
	if (!NavigationComponent)
	{
#if WITH_EDITOR
		UE_VLOG(this, VUESVON, Error, TEXT("SVONMoveTo request failed due to missing SVONNavigationComponent"), *MoveRequest.ToString());
		UE_LOG(UESVON, Error, TEXT("SVONMoveTo request failed due to missing SVONNavigationComponent on the pawn"));
		return;
#endif
	}

	// Use the path instance from the NavigationComponent
	SVONPath = NavigationComponent->GetPath();
}

void UAITask_SVONMoveTo::SetContinuousGoalTracking(bool bEnable)
{
	bUseContinuousTracking = bEnable;
}


void UAITask_SVONMoveTo::TickTask(float DeltaTime)
{
	if (AsyncTaskComplete)
		HandleAsyncPathTaskComplete();
}

void UAITask_SVONMoveTo::FinishMoveTask(EPathFollowingResult::Type Result)
{
	if (MoveRequestID.IsValid())
	{
		auto PathFollowingComponent = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
		if (PathFollowingComponent && PathFollowingComponent->GetStatus() != EPathFollowingStatus::Idle)
		{
			ResetObservers();
			PathFollowingComponent->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, MoveRequestID);
		}
	}

	MoveResult = Result;
	EndTask();

	if (Result == EPathFollowingResult::Invalid)	
		OnRequestFailed.Broadcast();
	else
		OnMoveFinished.Broadcast(Result, OwnerController);
}

void UAITask_SVONMoveTo::Activate()
{
	Super::Activate();

	UE_CVLOG(bUseContinuousTracking, GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("Continuous goal tracking requested, moving to: %s"),
		MoveRequest.IsMoveToActorRequest() ? TEXT("actor => looping successful moves!") : TEXT("location => will NOT loop"));

	MoveRequestID = FAIRequestID::InvalidRequest;
	ConditionalPerformMove();
}

void UAITask_SVONMoveTo::ConditionalPerformMove()
{
	if (MoveRequest.IsUsingPathfinding() && OwnerController && OwnerController->ShouldPostponePathUpdates())
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> can't path right now, waiting..."), *GetName());
		OwnerController->GetWorldTimerManager().SetTimer(MoveRetryTimerHandle, this, &UAITask_SVONMoveTo::ConditionalPerformMove, 0.2f, false);
	}
	else
	{
		MoveRetryTimerHandle.Invalidate();
		PerformMove();
	}
}

void UAITask_SVONMoveTo::PerformMove()
{
	// Prepare the first move first (check for early out)
	CheckPathPreConditions();

	ResetObservers();
	ResetTimers();
	ResetPaths();

	if (Result.Code == ESVONPathfindingRequestResult::SPRR_AlreadyAtGoal)
	{
		MoveRequestID = Result.MoveId;
		OnRequestFinished(Result.MoveId, FPathFollowingResult(EPathFollowingResult::Success, FPathFollowingResultFlags::AlreadyAtGoal));
	}

	// If we're ready to path, then request the path
	if (Result.Code == ESVONPathfindingRequestResult::SPRR_ReadyToPath)
	{
		bUseAsyncPathFinding ? RequestPathAsync() : RequestPath();

		switch (Result.Code)
		{
		case ESVONPathfindingRequestResult::SPRR_Failed:
			FinishMoveTask(EPathFollowingResult::Invalid);
			break;

		case ESVONPathfindingRequestResult::SPRR_Success: // Non-Async pathfinding
			MoveRequestID = Result.MoveId;
			if(IsFinished())
				UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("%s> re-Activating Finished task!"), *GetName());
			RequestMove(); // Start the move
			break;

		case ESVONPathfindingRequestResult::SPRR_Deferred: // Async
			MoveRequestID = Result.MoveId;
			AsyncTaskComplete = false;
			break;

		default:
			checkNoEntry();
			break;
		}
	}
}

void UAITask_SVONMoveTo::Pause()
{
	if (OwnerController && MoveRequestID.IsValid())
		OwnerController->PauseMove(MoveRequestID);

	ResetTimers();
	Super::Pause();
}

void UAITask_SVONMoveTo::Resume()
{
	Super::Resume();

	if (!MoveRequestID.IsValid() || (OwnerController && !OwnerController->ResumeMove(MoveRequestID)))
	{
		UE_CVLOG(MoveRequestID.IsValid(), GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> Resume move failed, starting new one."), *GetName());
		ConditionalPerformMove();
	}
}

void UAITask_SVONMoveTo::SetObservedPath(FNavPathSharedPtr Path)
{
	if (PathUpdateDelegateHandle.IsValid() && this->Path.IsValid())
		this->Path->RemoveObserver(PathUpdateDelegateHandle);

	PathUpdateDelegateHandle.Reset();

	this->Path = Path;
	if (this->Path.IsValid())
	{
		// disable auto repaths, it will be handled by move task to include ShouldPostponePathUpdates condition
		this->Path->EnableRecalculationOnInvalidation(false);
		PathUpdateDelegateHandle = this->Path->AddObserver(FNavigationPath::FPathObserverDelegate::FDelegate::CreateUObject(this, &UAITask_SVONMoveTo::OnPathEvent));
	}
}

void UAITask_SVONMoveTo::CheckPathPreConditions()
{
#if WITH_EDITOR
	UE_VLOG(this, VUESVON, Log, TEXT("SVONMoveTo: %s"), *MoveRequest.ToString());
#endif

	Result.Code = ESVONPathfindingRequestResult::SPRR_Failed;
	NavigationComponent = Cast<USVONNavigationComponent>(GetOwnerActor()->GetComponentByClass(USVONNavigationComponent::StaticClass()));
	if (!NavigationComponent)
	{
#if WITH_EDITOR
		UE_VLOG(this, VUESVON, Error, TEXT("SVONMoveTo request failed due missing SVONNavComponent on the pawn"), *MoveRequest.ToString());
		UE_LOG(UESVON, Error, TEXT("SVONMoveTo request failed due missing SVONNavComponent on the pawn"));
		return;
#endif
	}

	if (MoveRequest.IsValid() == false)
	{
#if WITH_EDITOR
		UE_VLOG(this, VUESVON, Error, TEXT("SVONMoveTo request failed due MoveRequest not being valid. Most probably desired Goal Actor not longer exists"), *MoveRequest.ToString());
		UE_LOG(UESVON, Error, TEXT("SVONMoveTo request failed due MoveRequest not being valid. Most probably desired Goal Actor not longer exists"));
#endif
		return;
	}

	if (!OwnerController->GetPathFollowingComponent())
	{
#if WITH_EDITOR
		UE_VLOG(this, VUESVON, Error, TEXT("SVONMoveTo request failed due missing PathFollowingComponent"));
		UE_LOG(UESVON, Error, TEXT("SVONMoveTo request failed due missing PathFollowingComponent"));
#endif
		return;
	}

	bool bCanRequestMove = true;
	bool bAlreadyAtGoal = false;

	if (!MoveRequest.IsMoveToActorRequest())
	{
		if (MoveRequest.GetGoalLocation().ContainsNaN() || FAISystem::IsValidLocation(MoveRequest.GetGoalLocation()) == false)
		{
#if WITH_EDITOR
			UE_VLOG(this, VUESVON, Error, TEXT("SVONMoveTo: Destination is not valid! Goal(%s)"), TEXT_AI_LOCATION(MoveRequest.GetGoalLocation()));
			UE_LOG(UESVON, Error, TEXT("SVONMoveTo: Destination is not valid! Goal(%s)"));
#endif
			bCanRequestMove = false;
		}

		bAlreadyAtGoal = bCanRequestMove && OwnerController->GetPathFollowingComponent()->HasReached(MoveRequest);
	}
	else
		bAlreadyAtGoal = bCanRequestMove && OwnerController->GetPathFollowingComponent()->HasReached(MoveRequest);

	if (bAlreadyAtGoal)
	{
#if WITH_EDITOR
		UE_VLOG(this, VUESVON, Log, TEXT("SVONMoveTo: already at goal!"));
		UE_LOG(UESVON, Log, TEXT("SVONMoveTo: already at goal!"));
#endif
		Result.MoveId = OwnerController->GetPathFollowingComponent()->RequestMoveWithImmediateFinish(EPathFollowingResult::Success);
		Result.Code = ESVONPathfindingRequestResult::SPRR_AlreadyAtGoal;
	}

	if (bCanRequestMove)
		Result.Code = ESVONPathfindingRequestResult::SPRR_ReadyToPath;

	return;
}

void UAITask_SVONMoveTo::RequestPath()
{
	Result.Code = ESVONPathfindingRequestResult::SPRR_Failed;

#if WITH_EDITOR
	UE_VLOG(this, VUESVON, Log, TEXT("SVONMoveTo: Requesting Synchronous pathfinding!"));
	UE_LOG(UESVON, Error, TEXT("SVONMoveTo: Requesting Synchronous pathfinding!"));
#endif

	if (NavigationComponent->FindPathImmediate(NavigationComponent->GetPawnLocation(), MoveRequest.IsMoveToActorRequest() ? MoveRequest.GetGoalActor()->GetActorLocation() : MoveRequest.GetGoalLocation(), &SVONPath))
		Result.Code = ESVONPathfindingRequestResult::SPRR_Success;

	return;
}

void UAITask_SVONMoveTo::RequestPathAsync()
{
	Result.Code = ESVONPathfindingRequestResult::SPRR_Failed;

	// Fail if no nav component
	auto SVONNavigationComponent = Cast<USVONNavigationComponent>(GetOwnerActor()->GetComponentByClass(USVONNavigationComponent::StaticClass()));
	if (!SVONNavigationComponent)
		return;

	AsyncTaskComplete = false;

	// Request the async path
	SVONNavigationComponent->FindPathAsync(NavigationComponent->GetPawnLocation(), MoveRequest.IsMoveToActorRequest() ? MoveRequest.GetGoalActor()->GetActorLocation() : MoveRequest.GetGoalLocation(), AsyncTaskComplete, &SVONPath);

	Result.Code = ESVONPathfindingRequestResult::SPRR_Deferred;
}

void UAITask_SVONMoveTo::RequestMove()
{
	Result.Code = ESVONPathfindingRequestResult::SPRR_Failed;

	LogPathHelper();

	// Copy the SVO path into a regular path for now, until we implement our own path follower.
	SVONPath->CreateNavigationPath(*Path);
	Path->MarkReady();

	auto PathFollowingComponent = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
	if (!PathFollowingComponent)
	{
		FinishMoveTask(EPathFollowingResult::Invalid);
		return;
	}

	PathFinishDelegateHandle = PathFollowingComponent->OnRequestFinished.AddUObject(this, &UAITask_SVONMoveTo::OnRequestFinished);
	SetObservedPath(Path);

	const FAIRequestID RequestID = Path->IsValid() ? OwnerController->RequestMove(MoveRequest, Path) : FAIRequestID::InvalidRequest;

	if (RequestID.IsValid())
	{
#if WITH_EDITOR
		UE_VLOG(this, VUESVON, Log, TEXT("SVON Pathfinding successful, moving"));
		UE_LOG(UESVON, Log, TEXT("SVON Pathfinding successful, moving"));
#endif
		Result.MoveId = RequestID;
		Result.Code = ESVONPathfindingRequestResult::SPRR_Success;
	}

	if (Result.Code == ESVONPathfindingRequestResult::SPRR_Failed)
		Result.MoveId = OwnerController->GetPathFollowingComponent()->RequestMoveWithImmediateFinish(EPathFollowingResult::Invalid);
}

void UAITask_SVONMoveTo::HandleAsyncPathTaskComplete()
{
	Result.Code = ESVONPathfindingRequestResult::SPRR_Success;
	RequestMove(); // Request the move
	AsyncTaskComplete = false; // Flag that we've processed the task
}

void UAITask_SVONMoveTo::ResetPaths()
{
	if(Path.IsValid())
		Path->ResetForRepath();

	if(SVONPath.IsValid())
		SVONPath->ResetForRepath();
}

void UAITask_SVONMoveTo::ResetObservers()
{
	if (Path.IsValid())
		Path->DisableGoalActorObservation();

	if (PathFinishDelegateHandle.IsValid())
	{
		auto PathFollowingComponent = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
		if (PathFollowingComponent)
			PathFollowingComponent->OnRequestFinished.Remove(PathFinishDelegateHandle);

		PathFinishDelegateHandle.Reset();
	}

	if (PathUpdateDelegateHandle.IsValid())
	{
		if (Path.IsValid())
			Path->RemoveObserver(PathUpdateDelegateHandle);

		PathUpdateDelegateHandle.Reset();
	}
}

void UAITask_SVONMoveTo::ResetTimers()
{
	if (MoveRetryTimerHandle.IsValid())
	{
		if (OwnerController)
			OwnerController->GetWorldTimerManager().ClearTimer(MoveRetryTimerHandle);

		MoveRetryTimerHandle.Invalidate();
	}

	if (PathRetryTimerHandle.IsValid())
	{
		if (OwnerController)
			OwnerController->GetWorldTimerManager().ClearTimer(PathRetryTimerHandle);

		PathRetryTimerHandle.Invalidate();
	}
}

void UAITask_SVONMoveTo::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);

	ResetObservers();
	ResetTimers();

	if (MoveRequestID.IsValid())
	{
		auto PathFollowingComponent = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
		if (PathFollowingComponent && PathFollowingComponent->GetStatus() != EPathFollowingStatus::Idle)
			PathFollowingComponent->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, MoveRequestID);
	}

	// clear the shared pointer now to make sure other systems
	// don't think this path is still being used
	Path.Reset();
	SVONPath.Reset();
}

void UAITask_SVONMoveTo::OnRequestFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (RequestID == this->Result.MoveId)
	{
		if (Result.HasFlag(FPathFollowingResultFlags::UserAbort) && Result.HasFlag(FPathFollowingResultFlags::NewRequest) && !Result.HasFlag(FPathFollowingResultFlags::ForcedScript))
		{
			UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> ignoring OnRequestFinished, move was aborted by new request"), *GetName());
		}
		else
		{
			// reset request Id, FinishMoveTask doesn't need to update path following's state
			this->Result.MoveId = FAIRequestID::InvalidRequest;

			if (bUseContinuousTracking && MoveRequest.IsMoveToActorRequest() && Result.IsSuccess())
			{
				UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> received OnRequestFinished and goal tracking is active! Moving again in next tick"), *GetName());
				GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UAITask_SVONMoveTo::PerformMove);
			}
			else
				FinishMoveTask(Result.Code);
		}
	}
	else if (IsActive())
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Warning, TEXT("%s> received OnRequestFinished with not matching RequestID!"), *GetName());
	}
}

void UAITask_SVONMoveTo::OnPathEvent(FNavigationPath* InPath, ENavPathEvent::Type Event)
{
	const static UEnum* NavPathEventEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ENavPathEvent"));
	UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> Path event: %s"), *GetName(), *NavPathEventEnum->GetNameStringByValue(Event));

	switch (Event)
	{
	case ENavPathEvent::NewPath:
	case ENavPathEvent::UpdatedDueToGoalMoved:
	case ENavPathEvent::UpdatedDueToNavigationChanged:
		if (InPath && InPath->IsPartial() && !MoveRequest.IsUsingPartialPaths())
		{
			UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT(">> partial path is not allowed, aborting"));
			UPathFollowingComponent::LogPathHelper(OwnerController, InPath, MoveRequest.GetGoalActor());
			FinishMoveTask(EPathFollowingResult::Aborted);
		}
#if ENABLE_VISUAL_LOG
		else if (!IsActive())
		{
			UPathFollowingComponent::LogPathHelper(OwnerController, InPath, MoveRequest.GetGoalActor());
		}
#endif // ENABLE_VISUAL_LOG
		break;

	case ENavPathEvent::Invalidated:
		ConditionalUpdatePath();
		break;

	case ENavPathEvent::Cleared:
	case ENavPathEvent::RePathFailed:
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT(">> no path, aborting!"));
		FinishMoveTask(EPathFollowingResult::Aborted);
		break;

	case ENavPathEvent::MetaPathUpdate:
	default:
		break;
	}
}

void UAITask_SVONMoveTo::ConditionalUpdatePath()
{
	// mark this path as waiting for repath so that PathFollowingComponent doesn't abort the move while we 
	// micro manage repathing moment
	// note that this flag fill get cleared upon repathing end
	if (Path.IsValid())
		Path->SetManualRepathWaiting(true);

	if (MoveRequest.IsUsingPathfinding() && OwnerController && OwnerController->ShouldPostponePathUpdates())
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> can't path right now, waiting..."), *GetName());
		OwnerController->GetWorldTimerManager().SetTimer(PathRetryTimerHandle, this, &UAITask_SVONMoveTo::ConditionalUpdatePath, 0.2f, false);
	}
	else
	{
		PathRetryTimerHandle.Invalidate();

		auto NavData = Path.IsValid() ? Path->GetNavigationDataUsed() : nullptr;
		if (NavData)
			NavData->RequestRePath(Path, ENavPathUpdateType::NavigationChanged);
		else
		{
			UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> unable to repath, aborting!"), *GetName());
			FinishMoveTask(EPathFollowingResult::Aborted);
		}
	}
}

void UAITask_SVONMoveTo::LogPathHelper()
{
#if WITH_EDITOR
#if ENABLE_VISUAL_LOG
	auto SVONNavigationComponent = Cast<USVONNavigationComponent>(GetOwnerActor()->GetComponentByClass(USVONNavigationComponent::StaticClass()));
	if (!SVONNavigationComponent)
		return;

	auto& VisualLogger = FVisualLogger::Get();
	if (VisualLogger.IsRecording() && SVONPath && SVONPath->GetPathPoints().Num() > 0)
	{
		auto Entry = VisualLogger.GetEntryToWrite(OwnerController->GetPawn(), OwnerController->GetPawn()->GetWorld()->TimeSeconds);
		if (Entry)
		{
			for (auto i = 0; i < SVONPath->GetPathPoints().Num(); i++)
			{
				if (i == 0 || i == SVONPath->GetPathPoints().Num() - 1)
					continue;

				const auto& Point = SVONPath->GetPathPoints()[i];
				auto Size = 0.0f;
				if (Point.Layer == 0)
					Size = SVONNavigationComponent->GetCurrentVolume()->GetVoxelSize(0) * 0.25f;
				else
					Size = SVONNavigationComponent->GetCurrentVolume()->GetVoxelSize(Point.Layer - 1);

				UE_VLOG_BOX(OwnerController->GetPawn(), UESVON, Verbose, FBox(Point.Location + FVector(Size * 0.5f), Point.Location - FVector(Size * 0.5f)), FColor::Black, TEXT_EMPTY);
			}
		}
	}
#endif // ENABLE_VISUAL_LOG
#endif // WITH_EDITOR
}
