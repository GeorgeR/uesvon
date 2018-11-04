#include "SVONVolumeDetails.h"

#include "SVONVolume.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "Components/BrushComponent.h"

#define LOCTEXT_NAMESPACE "SVONVolumeDetails"

TSharedRef<IDetailCustomization> FSVONVolumeDetails::MakeInstance()
{
	return MakeShareable(new FSVONVolumeDetails);
}

void FSVONVolumeDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	auto PrimaryTickProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UActorComponent, PrimaryComponentTick));

	// Defaults only show tick properties
	if (PrimaryTickProperty->IsValidHandle() && DetailBuilder.HasClassDefaultObject())
	{
		auto& TickCategory = DetailBuilder.EditCategory("ComponentTick");

		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bStartWithTickEnabled)));
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, TickInterval)));
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bTickEvenWhenPaused)), EPropertyLocation::Advanced);
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bAllowTickOnDedicatedServer)), EPropertyLocation::Advanced);
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, TickGroup)), EPropertyLocation::Advanced);
	}

	PrimaryTickProperty->MarkHiddenByCustomization();

	auto& NavigationCategory = DetailBuilder.EditCategory("SVON");

	auto ShowVoxelProperty = DetailBuilder.GetProperty("bShowVoxels");
	auto ShowVoxelLeafProperty = DetailBuilder.GetProperty("bShowLeafVoxels");
	auto ShowMortonCodesProperty = DetailBuilder.GetProperty("bShowMortonCodes");
	auto ShowNeighborLinksProperty = DetailBuilder.GetProperty("bShowNeighborLinks");
	auto ShowParentChildLinksProperty = DetailBuilder.GetProperty("myShowParentChildLinks");
	auto VoxelPowerProperty = DetailBuilder.GetProperty("VoxelPower");
	auto CollisionChannelProperty = DetailBuilder.GetProperty("CollisionChannel");
	auto ClearanceProperty = DetailBuilder.GetProperty("Clearance");
	
	ShowVoxelProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Voxels", "Debug Voxels"));
	ShowVoxelLeafProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Leaf Voxels", "Debug Leaf Voxels"));
	ShowMortonCodesProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Morton Codes", "Debug Morton Codes"));
	ShowNeighborLinksProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Links", "Debug Links"));
	ShowParentChildLinksProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Parent Child Links", "Parent Child Links"));
	VoxelPowerProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Layers", "Layers"));
	VoxelPowerProperty->SetInstanceMetaData("UIMin", TEXT("1"));
	VoxelPowerProperty->SetInstanceMetaData("UIMax", TEXT("12"));
	CollisionChannelProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Collision Channel", "Collision Channel"));
	ClearanceProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Clearance", "Clearance"));

	NavigationCategory.AddProperty(VoxelPowerProperty);
	NavigationCategory.AddProperty(CollisionChannelProperty);
	NavigationCategory.AddProperty(ClearanceProperty);

	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailBuilder.GetSelectedObjects();

	for (int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex)
	{
		const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
		if (CurrentObject.IsValid())
		{
			auto CurrentVolume = Cast<ASVONVolume>(CurrentObject.Get());
			if (CurrentVolume != nullptr)
			{
				Volume = CurrentVolume;
				break;
			}
		}
	}

	DetailBuilder.EditCategory("SVON")
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

#undef LOCTEXT_NAMESPACE