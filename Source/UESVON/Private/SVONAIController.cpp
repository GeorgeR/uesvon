#include "SVONAIController.h"

#include "VisualLogger/VisualLogger.h"
#include "NavigationSystem.h"
#include "SVONNavigationComponent.h"
#include "Tasks/AITask.h"
#include "Kismet/GameplayStatics.h"
#include "DisplayDebugHelpers.h"

DECLARE_CYCLE_STAT(TEXT("SVONMoveTo"), STAT_SVONMoveTo, STATGROUP_AI);

ASVONAIController::ASVONAIController()
{
	SVONNavComponent = CreateDefaultSubobject<USVONNavigationComponent>(TEXT("SVONNavigationComponent"));
    
	NavPath = MakeShareable<FSVONNavigationPath>(new FSVONNavigationPath());
}

FPathFollowingRequestResult ASVONAIController::MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath /*= nullptr*/)
{
	// both MoveToActor and MoveToLocation can be called from blueprints/script and should keep only single movement request at the same time.
	// this function is entry point of all movement mechanics - do NOT abort in here, since movement may be handled by AITasks, which support stacking 

//	SCOPE_CYCLE_COUNTER(STAT_SVONMoveTo);
//
//#if WITH_EDITOR
//	UE_VLOG(this, VUESVON, Log, TEXT("MoveTo: %s"), *MoveRequest.ToString());
//#endif

	FPathFollowingRequestResult ResultData;
	ResultData.Code = EPathFollowingRequestResult::Failed;

//	if (MoveRequest.IsValid() == false)
//	{
//#if WITH_EDITOR
//		UE_VLOG(this, VUESVON, Error, TEXT("MoveTo request failed due MoveRequest not being valid. Most probably desireg Goal Actor not longer exists"), *MoveRequest.ToString());
//#endif
//		return ResultData;
//	}
//
//	if (GetPathFollowingComponent() == nullptr)
//	{
//#if WITH_EDITOR
//		UE_VLOG(this, VUESVON, Error, TEXT("MoveTo request failed due missing PathFollowingComponent"));
//#endif
//		return ResultData;
//	}
//
//	ensure(MoveRequest.GetNavigationFilter() || !DefaultNavigationFilterClass);
//
//	bool bCanRequestMove = true;
//	bool bAlreadyAtGoal = false;
//
//	if (!MoveRequest.IsMoveToActorRequest())
//	{
//		if (MoveRequest.GetGoalLocation().ContainsNaN() || FAISystem::IsValidLocation(MoveRequest.GetGoalLocation()) == false)
//		{
//#if WITH_EDITOR
//			UE_VLOG(this, VUESVON, Error, TEXT("AAIController::MoveTo: Destination is not valid! Goal(%s)"), TEXT_AI_LOCATION(MoveRequest.GetGoalLocation()));
//#endif
//			bCanRequestMove = false;
//		}
//
//		bAlreadyAtGoal = bCanRequestMove && GetPathFollowingComponent()->HasReached(MoveRequest);
//	}
//	else
//		bAlreadyAtGoal = bCanRequestMove && GetPathFollowingComponent()->HasReached(MoveRequest);
//
//	if (bAlreadyAtGoal)
//	{
//#if WITH_EDITOR
//		UE_VLOG(this, VUESVON, Log, TEXT("MoveTo: already at goal!"));
//#endif
//		ResultData.MoveId = GetPathFollowingComponent()->RequestMoveWithImmediateFinish(EPathFollowingResult::Success);
//		ResultData.Code = EPathFollowingRequestResult::AlreadyAtGoal;
//	}
//	else if (bCanRequestMove)
//	{
//		SVONNavComponent->FindPathImmediate(GetPawn()->GetActorLocation(), MoveRequest.IsMoveToActorRequest() ? MoveRequest.GetGoalActor()->GetActorLocation() : MoveRequest.GetGoalLocation(), &NavPath);
//
//		const FAIRequestID RequestID = NavPath.IsValid() ? RequestMove(MoveRequest, NavPath) : FAIRequestID::InvalidRequest;
//
//		if(RequestID.IsValid())
//		{
//#if WITH_EDITOR
//			UE_VLOG(this, VUESVON, Log, TEXT("SVON Pathfinding successful, moving"));
//#endif
//			*OutPath = NavPath;
//			bAllowStrafe = MoveRequest.CanStrafe();
//			ResultData.MoveId = RequestID;
//			ResultData.Code = EPathFollowingRequestResult::RequestSuccessful;
//		}	
//	}
//
//	if (ResultData.Code == EPathFollowingRequestResult::Failed)
//		ResultData.MoveId = GetPathFollowingComponent()->RequestMoveWithImmediateFinish(EPathFollowingResult::Invalid);
//
	return ResultData;
}
