#include "SVONVolumeDetails.h"

#include "SVONVolumeActor.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "Components/BrushComponent.h"

#define LOCTEXT_NAMESPACE "SVONVolumeDetails"

TSharedRef<IDetailCustomization> FSVONVolumeDetails::MakeInstance()
{
	return MakeShareable(new FSVONVolumeDetails);
}

void FSVONVolumeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	auto PrimaryTickProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UActorComponent, PrimaryComponentTick));

	// Defaults only show tick properties
	if (PrimaryTickProperty->IsValidHandle() && DetailBuilder.HasClassDefaultObject())
	{
		auto& TickCategory = DetailBuilder.EditCategory(TEXT("ComponentTick"));

		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bStartWithTickEnabled)));
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, TickInterval)));
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bTickEvenWhenPaused)), EPropertyLocation::Advanced);
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bAllowTickOnDedicatedServer)), EPropertyLocation::Advanced);
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, TickGroup)), EPropertyLocation::Advanced);
	}

	PrimaryTickProperty->MarkHiddenByCustomization();

	auto& NavigationCategory = DetailBuilder.EditCategory(TEXT("SVON"));

	auto DebugDistanceProperty = DetailBuilder.GetProperty(TEXT("DebugDistance"));
	auto ShowVoxelProperty = DetailBuilder.GetProperty(TEXT("bShowVoxels"));
	auto ShowVoxelLeafProperty = DetailBuilder.GetProperty(TEXT("bShowLeafVoxels"));
	auto ShowMortonCodesProperty = DetailBuilder.GetProperty(TEXT("bShowMortonCodes"));
	auto ShowNeighborLinksProperty = DetailBuilder.GetProperty(TEXT("bShowNeighborLinks"));
	auto ShowParentChildLinksProperty = DetailBuilder.GetProperty(TEXT("bShowParentChildLinks"));
	auto VoxelPowerProperty = DetailBuilder.GetProperty(TEXT("VoxelPower"));
	auto CollisionChannelProperty = DetailBuilder.GetProperty(TEXT("CollisionChannel"));
	auto ClearanceProperty = DetailBuilder.GetProperty(TEXT("Clearance"));
	auto GenerationStrategyProperty = DetailBuilder.GetProperty(TEXT("GenerationStrategy"));
	auto NumLayersProperty = DetailBuilder.GetProperty(TEXT("NumLayers"));
	auto NumBytesProperty = DetailBuilder.GetProperty(TEXT("NumBytes"));

	DebugDistanceProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Distance", "Debug Distance"));
	ShowVoxelProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Voxels", "Debug Voxels"));
	ShowVoxelLeafProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Leaf Voxels", "Debug Leaf Voxels"));
	ShowMortonCodesProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Morton Codes", "Debug Morton Codes"));
	ShowNeighborLinksProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Links", "Debug Links"));
	ShowParentChildLinksProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Parent Child Links", "Parent Child Links"));
	VoxelPowerProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Layers", "Layers"));
	VoxelPowerProperty->SetInstanceMetaData(TEXT("UIMin"), TEXT("1"));
	VoxelPowerProperty->SetInstanceMetaData(TEXT("UIMax"), TEXT("12"));
	CollisionChannelProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Collision Channel", "Collision Channel"));
	ClearanceProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Clearance", "Clearance"));
	GenerationStrategyProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Generation Strategy", "Generation Strategy"));
	NumLayersProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Num Layers", "Num Layers"));
	NumBytesProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Num Bytes", "Num Bytes"));

	NavigationCategory.AddProperty(VoxelPowerProperty);
	NavigationCategory.AddProperty(CollisionChannelProperty);
	NavigationCategory.AddProperty(ClearanceProperty);
	NavigationCategory.AddProperty(GenerationStrategyProperty);
	NavigationCategory.AddProperty(NumLayersProperty);
	NavigationCategory.AddProperty(NumBytesProperty);

	const auto& SelectedObjects = DetailBuilder.GetSelectedObjects();

	for (auto ObjectIdx = 0; ObjectIdx < SelectedObjects.Num(); ++ObjectIdx)
	{
		const auto& CurrentObject = SelectedObjects[ObjectIdx];
		if (CurrentObject.IsValid())
		{
            auto* CurrentVolume = Cast<ASVONVolumeActor>(CurrentObject.Get());
			if (CurrentVolume != nullptr)
			{
				Volume = CurrentVolume;
				break;
			}
		}
	}

	DetailBuilder.EditCategory(TEXT("SVON"))
		.AddCustomRow(NSLOCTEXT("SVO Volume", "Generate", "Generate"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		    .Text(NSLOCTEXT("SVO Volume", "Generate", "Generate"))
		]
	    .ValueContent()
		.MaxDesiredWidth(125.f)
		.MinDesiredWidth(125.f)
		[
			SNew(SButton)
			.ContentPadding(2)
		    .VAlign(VAlign_Center)
		    .HAlign(HAlign_Center)
		    .OnClicked(this, &FSVONVolumeDetails::OnUpdateVolume)
		    [
			    SNew(STextBlock)
			    .Font(IDetailLayoutBuilder::GetDetailFont())
		        .Text(NSLOCTEXT("SVO Volume", "Generate", "Generate"))
		    ]
		];

	DetailBuilder.EditCategory(TEXT("SVON"))
		.AddCustomRow(NSLOCTEXT("SVO Volume", "Clear", "Clear"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		    .Text(NSLOCTEXT("SVO Volume", "Clear", "Clear"))
		]
	    .ValueContent()
		.MaxDesiredWidth(125.f)
		.MinDesiredWidth(125.f)
		[
			SNew(SButton)
			.ContentPadding(2)
		    .VAlign(VAlign_Center)
		    .HAlign(HAlign_Center)
		    .OnClicked(this, &FSVONVolumeDetails::OnClearVolumeClick)
		    [
			    SNew(STextBlock)
			    .Font(IDetailLayoutBuilder::GetDetailFont())
		        .Text(NSLOCTEXT("SVO Volume", "Clear", "Clear"))
		    ]
	    ];

	NavigationCategory.AddProperty(DebugDistanceProperty);
	NavigationCategory.AddProperty(ShowVoxelProperty);
	NavigationCategory.AddProperty(ShowVoxelLeafProperty);
	NavigationCategory.AddProperty(ShowMortonCodesProperty);
	NavigationCategory.AddProperty(ShowNeighborLinksProperty);
	NavigationCategory.AddProperty(ShowParentChildLinksProperty);
}

FReply FSVONVolumeDetails::OnUpdateVolume()
{
	if (Volume.IsValid())	
		Volume->Generate();

	return FReply::Handled();
}

FReply FSVONVolumeDetails::OnClearVolumeClick()
{
    if(Volume.IsValid())
		Volume->ClearData();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
