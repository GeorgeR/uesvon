#pragma once

#include "CoreMinimal.h"
#include "UnrealEd.h"

DECLARE_LOG_CATEGORY_EXTERN(UESVONEditor, All, All)

class FUESVONEditorModule
    : public IModuleInterface
{
public:
    void StartupModule() override;
    void ShutdownModule() override;
};
