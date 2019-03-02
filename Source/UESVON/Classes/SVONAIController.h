#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "SVONNavigationComponent.h"
#include "UESVON.h"
#include "SVONTypes.h"

#include "SVONAIController.generated.h"

//DECLARE_LOG_CATEGORY_EXTERN(LogAINavigation, Warning, All);

UCLASS()
class UESVON_API ASVONAIController 
    : public AAIController
{
	GENERATED_BODY()

public:
	ASVONAIController();

	/** Component used for moving along a path. */
	UPROPERTY(VisibleDefaultsOnly, Category = "SVON")
	USVONNavigationComponent* SVONNavComponent;

	FPathFollowingRequestResult MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath = nullptr) override;

private:
    FSVONNavPathSharedPtr NavPath;
};
