#include "SPABTViewport.h"

#include "AdvancedPreviewScene.h"
#include "EditorViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "PhysicsAssetBodyToolViewport"

void SPABTViewport::Construct(const FArguments& InArgs)
{
    PreviewScene = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());
    PreviewComponent = NewObject<USkeletalMeshComponent>();
    PreviewComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PreviewComponent->bSelectable = false;
    PreviewScene->AddComponent(PreviewComponent, FTransform::Identity);
    SEditorViewport::Construct(SEditorViewport::FArguments());
    AddOverlayWidget(SNew(STextBlock).Text(this, &SPABTViewport::GetStatsText).ColorAndOpacity(FLinearColor::White).ShadowOffset(FVector2D(1.f, 1.f)));
}

void SPABTViewport::SetPreviewAssets(USkeletalMesh* InSkeletalMesh, UPhysicsAsset* InPhysicsAsset)
{
    PhysicsAsset = InPhysicsAsset;
    if (PreviewComponent)
    {
        PreviewComponent->SetSkeletalMesh(InSkeletalMesh);
        PreviewComponent->SetPhysicsAsset(InPhysicsAsset);
        PreviewComponent->RefreshBoneTransforms();
        PreviewComponent->UpdateBounds();

        const FBox Bounds = PreviewComponent->Bounds.GetBox();
        if (Bounds.IsValid)
        {
            PreviewComponent->SetWorldLocation(FVector(-Bounds.GetCenter().X, -Bounds.GetCenter().Y, FMath::Max(0.f, -Bounds.Min.Z)));
            PreviewComponent->UpdateBounds();
        }
    }
    ApplyShowFlags();
    if (ViewportClient.IsValid())
    {
        ViewportClient->FocusViewportOnBox(PreviewComponent ? PreviewComponent->Bounds.GetBox() : FBox(EForceInit::ForceInit));
        ViewportClient->Invalidate();
    }
}

void SPABTViewport::SetSelectedBone(FName InBoneName)
{
    SelectedBone = InBoneName;
    if (ViewportClient.IsValid())
    {
        ViewportClient->Invalidate();
    }
}

TSharedRef<FEditorViewportClient> SPABTViewport::MakeEditorViewportClient()
{
    ViewportClient = MakeShared<FEditorViewportClient>(nullptr, PreviewScene.Get(), SharedThis(this));
    ViewportClient->SetViewMode(VMI_Lit);
    ViewportClient->SetRealtime(true);
    ViewportClient->bSetListenerPosition = false;
    ApplyShowFlags();
    return ViewportClient.ToSharedRef();
}

void SPABTViewport::ApplyShowFlags()
{
    if (!ViewportClient.IsValid())
    {
        return;
    }

    ViewportClient->EngineShowFlags.SetGrid(bShowGrid);
    ViewportClient->EngineShowFlags.SetSelectionOutline(true);
    ViewportClient->EngineShowFlags.SetCollision(bShowBodies);
    ViewportClient->EngineShowFlags.SetBones(bShowBones);
    if (PreviewScene.IsValid())
    {
        PreviewScene->SetFloorVisibility(bShowFloor);
    }
    ViewportClient->Invalidate();
}

TSharedPtr<SWidget> SPABTViewport::MakeViewportToolbar()
{
    return SNew(SBorder)
        .Padding(FMargin(6.f, 3.f))
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("Perspective", "Perspective"))]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("Lit", "Lit"))]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text_Lambda([this](){ return bShowBodies ? LOCTEXT("BodiesOn", "Bodies: On") : LOCTEXT("BodiesOff", "Bodies: Off"); }).OnClicked(this, &SPABTViewport::ToggleBodies)]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text_Lambda([this](){ return bShowBones ? LOCTEXT("BonesOn", "Bones: On") : LOCTEXT("BonesOff", "Bones: Off"); }).OnClicked(this, &SPABTViewport::ToggleBones)]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text_Lambda([this](){ return bShowFloor ? LOCTEXT("FloorOn", "Floor: On") : LOCTEXT("FloorOff", "Floor: Off"); }).OnClicked(this, &SPABTViewport::ToggleFloor)]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text_Lambda([this](){ return bShowGrid ? LOCTEXT("GridOn", "Grid: On") : LOCTEXT("GridOff", "Grid: Off"); }).OnClicked(this, &SPABTViewport::ToggleGrid)]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("Focus", "Focus")).OnClicked(this, &SPABTViewport::FocusPreview)]
        ];
}

FReply SPABTViewport::ToggleBodies() { bShowBodies = !bShowBodies; ApplyShowFlags(); return FReply::Handled(); }
FReply SPABTViewport::ToggleBones() { bShowBones = !bShowBones; ApplyShowFlags(); return FReply::Handled(); }
FReply SPABTViewport::ToggleFloor() { bShowFloor = !bShowFloor; ApplyShowFlags(); return FReply::Handled(); }
FReply SPABTViewport::ToggleGrid() { bShowGrid = !bShowGrid; ApplyShowFlags(); return FReply::Handled(); }
FReply SPABTViewport::FocusPreview()
{
    if (ViewportClient.IsValid() && PreviewComponent)
    {
        ViewportClient->FocusViewportOnBox(PreviewComponent->Bounds.GetBox());
        ViewportClient->Invalidate();
    }
    return FReply::Handled();
}

FText SPABTViewport::GetStatsText() const
{
    const UPhysicsAsset* Asset = PhysicsAsset.Get();
    if (!Asset)
    {
        return LOCTEXT("NoPhysicsAssetStats", "No Physics Asset selected");
    }

    int32 PrimitiveCount = 0;
    for (const USkeletalBodySetup* Setup : Asset->SkeletalBodySetups)
    {
        if (Setup)
        {
            PrimitiveCount += Setup->AggGeom.BoxElems.Num();
            PrimitiveCount += Setup->AggGeom.SphereElems.Num();
            PrimitiveCount += Setup->AggGeom.SphylElems.Num();
            PrimitiveCount += Setup->AggGeom.ConvexElems.Num();
        }
    }

    return FText::Format(
        LOCTEXT("PhysicsAssetStats", "{0} Bodies\n{1} Primitives\n{2} Constraints"),
        FText::AsNumber(Asset->SkeletalBodySetups.Num()),
        FText::AsNumber(PrimitiveCount),
        FText::AsNumber(Asset->ConstraintSetup.Num())
    );
}

#undef LOCTEXT_NAMESPACE
