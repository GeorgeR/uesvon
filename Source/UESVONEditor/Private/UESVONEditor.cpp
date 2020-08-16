#include "UESVONEditor.h"

#include "PropertyEditorModule.h"

#include "SVONVolumeActor.h"
#include "SVONVolumeDetails.h"

IMPLEMENT_GAME_MODULE(FUESVONEditorModule, UESVONEditor);

DEFINE_LOG_CATEGORY(UESVONEditor)

#define LOCTEXT_NAMESPACE "UESVONEditor"

void FUESVONEditorModule::StartupModule()
{
	UE_LOG(UESVONEditor, Display, TEXT("UESVONEditorModule: Log Started"));

	auto& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    PropertyModule.RegisterCustomClassLayout(ASVONVolumeActor::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSVONVolumeDetails::MakeInstance));
}

void FUESVONEditorModule::ShutdownModule()
{
    auto& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.UnregisterCustomClassLayout(ASVONVolumeActor::StaticClass()->GetFName());

	UE_LOG(UESVONEditor, Display, TEXT("UESVONEditorModule: Log Ended"));
}

#undef LOCTEXT_NAMESPACE
