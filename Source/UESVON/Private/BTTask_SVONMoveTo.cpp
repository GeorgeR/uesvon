#include "BTTask_SVONMoveTo.h"

#include "GameFramework/Actor.h"
#include "AISystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "VisualLogger/VisualLogger.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AITask_SVONMoveTo.h"

UBTTask_SVONMoveTo::UBTTask_SVONMoveTo()
{
	NodeName = TEXT("SVON Move To");
	bUseGameplayTasks = GET_AI_CONFIG_VAR(bEnableBTAITasks);
	bNotifyTick = !bUseGameplayTasks;
	bNotifyTaskFinished = true;

	AcceptableRadius = GET_AI_CONFIG_VAR(AcceptanceRadius);
	bReachTestIncludesGoalRadius = bReachTestIncludesAgentRadius = GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap);
	bTrackMovingGoal = true;
	ObservedBlackboardValueTolerance = AcceptableRadius * 0.95f;
	bUseAsyncPathfinding = false;

	// accept only actors and vectors
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_SVONMoveTo, BlackboardKey), AActor::StaticClass());
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_SVONMoveTo, BlackboardKey));
}

EBTNodeResult::Type UBTTask_SVONMoveTo::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	auto NodeResult = EBTNodeResult::InProgress;

	auto Memory = reinterpret_cast<FBTSVONMoveToTaskMemory*>(NodeMemory);
	Memory->PreviousGoalLocation = FAISystem::InvalidLocation;
	Memory->MoveRequestID = FAIRequestID::InvalidRequest;

	auto Controller = OwnerComp.GetAIOwner();
	Memory->bWaitingForPath = bUseGameplayTasks ? false : Controller->ShouldPostponePathUpdates();
	if (!Memory->bWaitingForPath)
		NodeResult = PerformMoveTask(OwnerComp, NodeMemory);
	else
	{
		UE_VLOG(Controller, LogBehaviorTree, Log, TEXT("Pathfinding requests are freezed, waiting..."));
	}

	if (NodeResult == EBTNodeResult::InProgress && bObserveBlackboardValue)
	{
		auto BlackboardComponent = OwnerComp.GetBlackboardComponent();
		if (ensure(BlackboardComponent))
		{
			if (Memory->BBObserverDelegateHandle.IsValid())
			{
				UE_VLOG(Controller, LogBehaviorTree, Warning, TEXT("UBTTask_SVONMoveTo::ExecuteTask \'%s\' Old BBObserverDelegateHandle is still valid! Removing old Observer."), *GetNodeName());
				BlackboardComponent->UnregisterObserver(BlackboardKey.GetSelectedKeyID(), Memory->BBObserverDelegateHandle);
			}

			Memory->BBObserverDelegateHandle = BlackboardComponent->RegisterObserver(BlackboardKey.GetSelectedKeyID(), this, FOnBlackboardChangeNotification::CreateUObject(this, &UBTTask_SVONMoveTo::OnBlackboardValueChange));
		}
	}

	return NodeResult;
}

EBTNodeResult::Type UBTTask_SVONMoveTo::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	auto Memory = reinterpret_cast<FBTSVONMoveToTaskMemory*>(NodeMemory);
	if (!Memory->bWaitingForPath)
	{
		if (Memory->MoveRequestID.IsValid())
		{
			auto Controller = OwnerComp.GetAIOwner();
			if (Controller && Controller->GetPathFollowingComponent())
				Controller->GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, Memory->MoveRequestID);
		}
		else
		{
			Memory->bObserverCanFinishTask = false;
			auto MoveTask = Memory->Task.Get();
			if (MoveTask)
				MoveTask->ExternalCancel();
			else
			{
				UE_VLOG(&OwnerComp, LogBehaviorTree, Error, TEXT("Can't abort path following! bWaitingForPath:false, MoveRequestID:invalid, MoveTask:none!"));
			}
		}
	}

	return Super::AbortTask(OwnerComp, NodeMemory);
}

void UBTTask_SVONMoveTo::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	auto Memory = reinterpret_cast<FBTSVONMoveToTaskMemory*>(NodeMemory);
	Memory->Task.Reset();

	if (bObserveBlackboardValue)
	{
		auto BlackboardComponent = OwnerComp.GetBlackboardComponent();
		if (ensure(BlackboardComponent) && Memory->BBObserverDelegateHandle.IsValid())
			BlackboardComponent->UnregisterObserver(BlackboardKey.GetSelectedKeyID(), Memory->BBObserverDelegateHandle);

		Memory->BBObserverDelegateHandle.Reset();
	}

	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UBTTask_SVONMoveTo::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	auto Memory = (FBTSVONMoveToTaskMemory*)NodeMemory;
	if (Memory->bWaitingForPath && !OwnerComp.IsPaused())
	{
		auto Controller = OwnerComp.GetAIOwner();
		if (Controller && !Controller->ShouldPostponePathUpdates())
		{
			UE_VLOG(Controller, LogBehaviorTree, Log, TEXT("Pathfinding requests are unlocked!"));
			Memory->bWaitingForPath = false;

			const EBTNodeResult::Type NodeResult = PerformMoveTask(OwnerComp, NodeMemory);
			if (NodeResult != EBTNodeResult::InProgress)
				FinishLatentTask(OwnerComp, NodeResult);
		}
	}
}

uint16 UBTTask_SVONMoveTo::GetInstanceMemorySize() const
{
	return sizeof(FBTSVONMoveToTaskMemory);
}

void UBTTask_SVONMoveTo::PostLoad()
{
	Super::PostLoad();
}

void UBTTask_SVONMoveTo::OnGameplayTaskDeactivated(UGameplayTask& Task)
{
	// AI move task finished
	auto MoveTask = Cast<UAITask_SVONMoveTo>(&Task);
	if (MoveTask && MoveTask->GetAIController() && MoveTask->GetState() != EGameplayTaskState::Paused)
	{
		auto BehaviorComponent = GetBTComponentForTask(Task);
		if (BehaviorComponent)
		{
			auto NodeMemory = BehaviorComponent->GetNodeMemory(this, BehaviorComponent->FindInstanceContainingNode(this));
			auto Memory = reinterpret_cast<FBTSVONMoveToTaskMemory*>(NodeMemory);

			if (Memory->bObserverCanFinishTask && (MoveTask == Memory->Task))
			{
				const bool bSuccess = MoveTask->WasMoveSuccessful();
				FinishLatentTask(*BehaviorComponent, bSuccess ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
			}
		}
	}
}

void UBTTask_SVONMoveTo::OnMessage(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, FName Message, int32 RequestID, bool bSuccess)
{
	// AIMessage_RepathFailed means task has failed
	bSuccess &= (Message != UBrainComponent::AIMessage_RepathFailed);
	Super::OnMessage(OwnerComp, NodeMemory, Message, RequestID, bSuccess);
}

EBlackboardNotificationResult UBTTask_SVONMoveTo::OnBlackboardValueChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID)
{
	auto BehaviorComponent = Cast<UBehaviorTreeComponent>(Blackboard.GetBrainComponent());
	if (BehaviorComponent == nullptr)
		return EBlackboardNotificationResult::RemoveObserver;

	auto NodeMemory = BehaviorComponent->GetNodeMemory(this, BehaviorComponent->FindInstanceContainingNode(this));
	auto Memory = reinterpret_cast<FBTSVONMoveToTaskMemory*>(NodeMemory);

	const EBTTaskStatus::Type TaskStatus = BehaviorComponent->GetTaskStatus(this);
	if (TaskStatus != EBTTaskStatus::Active)
	{
		UE_VLOG(BehaviorComponent, LogBehaviorTree, Error, TEXT("BT MoveTo \'%s\' task observing BB entry while no longer being active!"), *GetNodeName());

		// resetting BBObserverDelegateHandle without unregistering observer since 
		// returning EBlackboardNotificationResult::RemoveObserver here will take care of that for us
		Memory->BBObserverDelegateHandle.Reset(); //-V595

		return EBlackboardNotificationResult::RemoveObserver;
	}

	// this means the move has already started. MyMemory->bWaitingForPath == true would mean we're waiting for right moment to start it anyway,
	// so we don't need to do anything due to BB value change 
	if (Memory != nullptr && Memory->bWaitingForPath == false && BehaviorComponent->GetAIOwner() != nullptr)
	{
		check(BehaviorComponent->GetAIOwner()->GetPathFollowingComponent());

		bool bUpdateMove = true;
		// check if new goal is almost identical to previous one
		if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
		{
			const FVector TargetLocation = Blackboard.GetValue<UBlackboardKeyType_Vector>(BlackboardKey.GetSelectedKeyID());
			bUpdateMove = (FVector::DistSquared(TargetLocation, Memory->PreviousGoalLocation) > FMath::Square(ObservedBlackboardValueTolerance));
		}

		if (bUpdateMove)
		{
			// don't abort move if using AI tasks - it will mess things up
			if (Memory->MoveRequestID.IsValid())
			{
				UE_VLOG(BehaviorComponent, LogBehaviorTree, Log, TEXT("Blackboard value for goal has changed, aborting current move request"));
				StopWaitingForMessages(*BehaviorComponent);
				BehaviorComponent->GetAIOwner()->GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::NewRequest, Memory->MoveRequestID, EPathFollowingVelocityMode::Keep);
			}

			if (!bUseGameplayTasks && BehaviorComponent->GetAIOwner()->ShouldPostponePathUpdates())
			{
				// NodeTick will take care of requesting move
				Memory->bWaitingForPath = true;
			}
			else
			{
				const EBTNodeResult::Type NodeResult = PerformMoveTask(*BehaviorComponent, NodeMemory);
				if (NodeResult != EBTNodeResult::InProgress)
					FinishLatentTask(*BehaviorComponent, NodeResult);
			}
		}
	}

	return EBlackboardNotificationResult::ContinueObserving;
}

void UBTTask_SVONMoveTo::DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{

}

FString UBTTask_SVONMoveTo::GetStaticDescription() const
{
	FString KeyDesc(TEXT("invalid"));
	if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass()
		|| BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
	{
		KeyDesc = BlackboardKey.SelectedKeyName.ToString();
	}

	return FString::Printf(TEXT("%s: %s"), *Super::GetStaticDescription(), *KeyDesc);
}

#if WITH_EDITOR
FName UBTTask_SVONMoveTo::GetNodeIconName() const
{
	return FName(TEXT("BTEditor.Graph.BTNode.Task.MoveTo.Icon"));
}

void UBTTask_SVONMoveTo::OnNodeCreated()
{

}
#endif

EBTNodeResult::Type UBTTask_SVONMoveTo::PerformMoveTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const auto BlackboardComponent = OwnerComp.GetBlackboardComponent();
	auto Memory = reinterpret_cast<FBTSVONMoveToTaskMemory*>(NodeMemory);
	auto Controller = OwnerComp.GetAIOwner();

	auto NodeResult = EBTNodeResult::Failed;
	if (Controller && BlackboardComponent)
	{
		FAIMoveRequest MoveRequest;
		MoveRequest.SetNavigationFilter(Controller->GetDefaultNavigationFilterClass());
		MoveRequest.SetAcceptanceRadius(AcceptableRadius);
		MoveRequest.SetReachTestIncludesAgentRadius(bReachTestIncludesAgentRadius);
		MoveRequest.SetReachTestIncludesGoalRadius(bReachTestIncludesGoalRadius);
		MoveRequest.SetUsePathfinding(true);

		if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass())
		{
			auto KeyValue = BlackboardComponent->GetValue<UBlackboardKeyType_Object>(BlackboardKey.GetSelectedKeyID());
			auto TargetActor = Cast<AActor>(KeyValue);
			if (TargetActor)
			{
				if (bTrackMovingGoal)
					MoveRequest.SetGoalActor(TargetActor);
				else
					MoveRequest.SetGoalLocation(TargetActor->GetActorLocation());
			}
			else
			{
				UE_VLOG(Controller, LogBehaviorTree, Warning, TEXT("UBTTask_MoveTo::ExecuteTask tried to go to actor while BB %s entry was empty"), *BlackboardKey.SelectedKeyName.ToString());
			}
		}
		else if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
		{
			const FVector TargetLocation = BlackboardComponent->GetValue<UBlackboardKeyType_Vector>(BlackboardKey.GetSelectedKeyID());
			MoveRequest.SetGoalLocation(TargetLocation);

			Memory->PreviousGoalLocation = TargetLocation;
		}

		if (MoveRequest.IsValid())
		{
			if (GET_AI_CONFIG_VAR(bEnableBTAITasks))
			{
				auto MoveTask = Memory->Task.Get();
				const bool bReuseExistingTask = (MoveTask != nullptr);

				MoveTask = PrepareMoveTask(OwnerComp, MoveTask, MoveRequest);
				if (MoveTask)
				{
					Memory->bObserverCanFinishTask = false;

					if (bReuseExistingTask)
					{
						if (MoveTask->IsActive())
						{
							UE_VLOG(Controller, LogBehaviorTree, Verbose, TEXT("\'%s\' reusing AITask %s"), *GetNodeName(), *MoveTask->GetName());
							MoveTask->ConditionalPerformMove();
						}
						else
						{
							UE_VLOG(Controller, LogBehaviorTree, Verbose, TEXT("\'%s\' reusing AITask %s, but task is not active - handing over move performing to task mechanics"), *GetNodeName(), *MoveTask->GetName());
						}
					}
					else
					{
						Memory->Task = MoveTask;
						UE_VLOG(Controller, LogBehaviorTree, Verbose, TEXT("\'%s\' task implementing move with task %s"), *GetNodeName(), *MoveTask->GetName());
						MoveTask->ReadyForActivation();
					}

					Memory->bObserverCanFinishTask = true;
					NodeResult = (MoveTask->GetState() != EGameplayTaskState::Finished) ? EBTNodeResult::InProgress :
						MoveTask->WasMoveSuccessful() ? EBTNodeResult::Succeeded :
						EBTNodeResult::Failed;
				}
			}
			else
			{
				FPathFollowingRequestResult RequestResult = Controller->MoveTo(MoveRequest);
				if (RequestResult.Code == EPathFollowingRequestResult::RequestSuccessful)
				{
					Memory->MoveRequestID = RequestResult.MoveId;
					WaitForMessage(OwnerComp, UBrainComponent::AIMessage_MoveFinished, RequestResult.MoveId);
					WaitForMessage(OwnerComp, UBrainComponent::AIMessage_RepathFailed);

					NodeResult = EBTNodeResult::InProgress;
				}
				else if (RequestResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
				{
					NodeResult = EBTNodeResult::Succeeded;
				}
			}
		}
	}

	return NodeResult;
}

UAITask_SVONMoveTo* UBTTask_SVONMoveTo::PrepareMoveTask(UBehaviorTreeComponent& OwnerComp, UAITask_SVONMoveTo* ExistingTask, FAIMoveRequest& MoveRequest)
{
	auto MoveTask = ExistingTask ? ExistingTask : NewBTAITask<UAITask_SVONMoveTo>(OwnerComp);
	if (MoveTask)
		MoveTask->SetUp(MoveTask->GetAIController(), MoveRequest, bUseAsyncPathfinding);

	return MoveTask;
}
