#include "SPABTViewport.h"

#include "AdvancedPreviewScene.h"
#include "EditorViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "SceneManagement.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "PhysicsAssetBodyToolViewport"

namespace
{
class FPABTViewportClient final : public FEditorViewportClient
{
public:
    FPABTViewportClient(FPreviewScene* InPreviewScene, const TSharedRef<SEditorViewport>& InViewport, SPABTViewport* InOwner)
        : FEditorViewportClient(nullptr, InPreviewScene, InViewport)
        , Owner(InOwner)
    {
    }

    void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override
    {
        FEditorViewportClient::Draw(View, PDI);
        if (!Owner || !PDI)
        {
            return;
        }

        USkeletalMeshComponent* Component = Owner->GetPreviewComponent();
        UPhysicsAsset* Asset = Owner->GetPhysicsAsset();
        if (!Component || !Asset)
        {
            return;
        }

        const FName SelectedBone = Owner->GetSelectedBone();
        for (const USkeletalBodySetup* Setup : Asset->SkeletalBodySetups)
        {
            if (!Setup)
            {
                continue;
            }

            const int32 BoneIndex = Component->GetBoneIndex(Setup->BoneName);
            if (BoneIndex == INDEX_NONE)
            {
                continue;
            }

            const FTransform BoneTM = Component->GetBoneTransform(BoneIndex);
            const bool bSelected = Setup->BoneName == SelectedBone;
            const FColor Color = bSelected ? FColor::Yellow : FColor::Cyan;
            const uint8 DepthPriority = bSelected ? SDPG_Foreground : SDPG_World;

            for (const FKBoxElem& Box : Setup->AggGeom.BoxElems)
            {
                const FTransform ShapeTM = Box.GetTransform() * BoneTM;
                DrawWireBox(PDI, ShapeTM.ToMatrixWithScale(), FBox(FVector(-Box.X, -Box.Y, -Box.Z) * 0.5f, FVector(Box.X, Box.Y, Box.Z) * 0.5f), Color, DepthPriority);
            }
            for (const FKSphereElem& Sphere : Setup->AggGeom.SphereElems)
            {
                const FTransform ShapeTM = Sphere.GetTransform() * BoneTM;
                DrawWireSphere(PDI, ShapeTM.GetLocation(), Color, Sphere.Radius, 24, DepthPriority);
            }
            for (const FKSphylElem& Capsule : Setup->AggGeom.SphylElems)
            {
                const FTransform ShapeTM = Capsule.GetTransform() * BoneTM;
                DrawWireCapsule(PDI, ShapeTM.GetLocation(), ShapeTM.GetUnitAxis(EAxis::X), ShapeTM.GetUnitAxis(EAxis::Y), ShapeTM.GetUnitAxis(EAxis::Z), Color, Capsule.Radius, Capsule.Length * 0.5f, 16, DepthPriority);
            }
            for (const FKConvexElem& Convex : Setup->AggGeom.ConvexElems)
            {
                const FTransform ShapeTM = Convex.GetTransform() * BoneTM;
                DrawWireBox(PDI, ShapeTM.ToMatrixWithScale(), Convex.ElemBox, Color, DepthPriority);
            }

            if (bSelected)
            {
                const FVector Origin = BoneTM.GetLocation();
                constexpr float AxisLength = 35.f;
                PDI->DrawLine(Origin, Origin + BoneTM.GetUnitAxis(EAxis::X) * AxisLength, FLinearColor::Red, SDPG_Foreground, 2.f);
                PDI->DrawLine(Origin, Origin + BoneTM.GetUnitAxis(EAxis::Y) * AxisLength, FLinearColor::Green, SDPG_Foreground, 2.f);
                PDI->DrawLine(Origin, Origin + BoneTM.GetUnitAxis(EAxis::Z) * AxisLength, FLinearColor::Blue, SDPG_Foreground, 2.f);
            }
        }

        for (const UPhysicsConstraintTemplate* Constraint : Asset->ConstraintSetup)
        {
            if (!Constraint)
            {
                continue;
            }
            const int32 Bone1 = Component->GetBoneIndex(Constraint->DefaultInstance.ConstraintBone1);
            const int32 Bone2 = Component->GetBoneIndex(Constraint->DefaultInstance.ConstraintBone2);
            if (Bone1 != INDEX_NONE && Bone2 != INDEX_NONE)
            {
                const FVector A = Component->GetBoneTransform(Bone1).TransformPosition(Constraint->DefaultInstance.Pos1);
                const FVector B = Component->GetBoneTransform(Bone2).TransformPosition(Constraint->DefaultInstance.Pos2);
                PDI->DrawLine(A, B, FLinearColor::Magenta, SDPG_Foreground, 2.f);
                DrawWireSphere(PDI, A, FColor::Magenta, 4.f, 8, SDPG_Foreground);
                DrawWireSphere(PDI, B, FColor::Magenta, 4.f, 8, SDPG_Foreground);
            }
        }
    }

private:
    SPABTViewport* Owner = nullptr;
};
}

void SPABTViewport::Construct(const FArguments& InArgs)
{
    PreviewScene = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());
    PreviewComponent = NewObject<USkeletalMeshComponent>();
    PreviewComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PreviewComponent->bSelectable = false;
    PreviewScene->AddComponent(PreviewComponent, FTransform::Identity);
    SEditorViewport::Construct(SEditorViewport::FArguments());
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
    ViewportClient = MakeShared<FPABTViewportClient>(PreviewScene.Get(), SharedThis(this), this);
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
            + SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right)[SNew(STextBlock).Text(this, &SPABTViewport::GetStatsText)]
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
