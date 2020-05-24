#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "Tasks/AITask.h"
#include "HAL/ThreadSafeBool.h"

#include "SVONTypes.h"
#include "SVONDefines.h"

#include "AITask_SVONMoveTo.generated.h"

class AAIController;
class USVONNavigationComponent;
struct FSVONNavigationPath;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSVONMoveTaskCompletedSignature, TEnumAsByte<EPathFollowingResult::Type>, Result, AAIController*, AIController);

UCLASS()
class UESVON_API UAITask_SVONMoveTo
	: public UAITask
{
	GENERATED_BODY()

public:
	UAITask_SVONMoveTo(const FObjectInitializer& ObjectInitializer);

	/** tries to start move request and handles retry timer */
	void ConditionalPerformMove();

	/** prepare move task for activation */
	void Setup(AAIController* Controller, const FAIMoveRequest& InMoveRequest, bool InbUseAsyncPathFinding);

	EPathFollowingResult::Type GetMoveResult() const { return MoveResult; }
	bool WasMoveSuccessful() const { return MoveResult == EPathFollowingResult::Success; }

	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (AdvancedDisplay = "AcceptanceRadius,StopOnOverlap,AcceptPartialPath,bUsePathfinding,bUseContinuosGoalTracking", DefaultToSelf = "Controller", BlueprintInternalUseOnly = "TRUE", DisplayName = "SVON Move To Location or Actor"))
	static UAITask_SVONMoveTo* SVONAIMoveTo(AAIController* Controller, FVector GoalLocation, bool bUseAsyncPathFinding, AActor* GoalActor = nullptr,
		float AcceptanceRadius = -1.0f, EAIOptionFlag::Type StopOnOverlap = EAIOptionFlag::Default, bool bLockAILogic = true, bool bUseContinuousGoalTracking = false);

	/** Allows custom move request tweaking. Note that all MoveRequest need to
	*	be performed before PerformMove is called. */
	FAIMoveRequest& GetMoveRequestRef() { return MoveRequest; }

	/** Switch task into continuous tracking mode: keep restarting move toward goal actor. Only pathfinding failure or external cancel will be able to stop this task. */
	void SetContinuousGoalTracking(bool bEnable);

	void TickTask(float DeltaTime) override;

protected:
	void LogPathHelper() const;

	FThreadSafeBool AsyncTaskComplete;
	bool bUseAsyncPathFinding;

	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnRequestFailed;

	UPROPERTY(BlueprintAssignable)
	FSVONMoveTaskCompletedSignature OnMoveFinished;

	/** parameters of move request */
	UPROPERTY()
	FAIMoveRequest MoveRequest;

	/** handle of path following's OnMoveFinished delegate */
	FDelegateHandle PathFinishDelegateHandle;

	/** handle of path's update event delegate */
	FDelegateHandle PathUpdateDelegateHandle;

	/** handle of active ConditionalPerformMove timer  */
	FTimerHandle MoveRetryTimerHandle;

	/** handle of active ConditionalUpdatePath timer */
	FTimerHandle PathRetryTimerHandle;

	/** request ID of path following's request */
	FAIRequestID MoveRequestID;

	/** currently followed path */
	FNavPathSharedPtr Path;

	FSVONNavPathSharedPtr SVONPath;

	TEnumAsByte<EPathFollowingResult::Type> MoveResult;
	uint8 bUseContinuousTracking : 1;

	virtual void Activate() override;
	virtual void OnDestroy(bool bIsOwnerFinished) override;

	virtual void Pause() override;
	virtual void Resume() override;

	/** finish task */
	void FinishMoveTask(EPathFollowingResult::Type InResult);

	/** stores path and starts observing its events */
	void SetObservedPath(FNavPathSharedPtr InPath);

	//FPathFollowingRequestResult RequestPath(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath = nullptr);
	FSVONPathfindingRequestResult Result;
	USVONNavigationComponent* NavigationComponent;

	void CheckPathPreConditions();

	void RequestPath();
	void RequestPathAsync();

	void RequestMove();

	void HandleAsyncPathTaskComplete();

	void ResetPaths() const;

	/** remove all delegates */
	virtual void ResetObservers();

	/** remove all timers */
	virtual void ResetTimers();

	/** tries to update invalidated path and handles retry timer */
	void ConditionalUpdatePath();

	/** start move request */
	virtual void PerformMove();

	/** event from followed path */
	virtual void OnPathEvent(FNavigationPath* InPath, ENavPathEvent::Type Event);

	/** event from path following */
	virtual void OnRequestFinished(FAIRequestID RequestID, const FPathFollowingResult& OutResult);
};
