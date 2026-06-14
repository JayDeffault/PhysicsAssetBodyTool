#include "SPABTViewport.h"

#include "AdvancedPreviewScene.h"
#include "EditorViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "SceneManagement.h"
#include "HitProxies.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "PhysicsAssetBodyToolViewport"


struct HPABTPrimitiveProxy final : public HHitProxy
{
    DECLARE_HIT_PROXY();
    HPABTPrimitiveProxy(FName InBoneName, EPABTViewportPrimitiveType InPrimitiveType, int32 InPrimitiveIndex)
        : HHitProxy(HPP_World)
        , BoneName(InBoneName)
        , PrimitiveType(InPrimitiveType)
        , PrimitiveIndex(InPrimitiveIndex)
    {
    }
    FName BoneName;
    EPABTViewportPrimitiveType PrimitiveType = EPABTViewportPrimitiveType::None;
    int32 PrimitiveIndex = INDEX_NONE;
};
IMPLEMENT_HIT_PROXY(HPABTPrimitiveProxy, HHitProxy);

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
            auto PrimitiveColor = [&](EPABTViewportPrimitiveType Type, int32 Index)
            {
                const bool bPrimitiveSelected = bSelected && Owner->GetSelectedPrimitiveType() == Type && Owner->GetSelectedPrimitiveIndex() == Index;
                return bPrimitiveSelected ? FColor::Yellow : Color;
            };
            auto PrimitiveDepth = [&](EPABTViewportPrimitiveType Type, int32 Index)
            {
                const bool bPrimitiveSelected = bSelected && Owner->GetSelectedPrimitiveType() == Type && Owner->GetSelectedPrimitiveIndex() == Index;
                return bPrimitiveSelected ? SDPG_Foreground : DepthPriority;
            };

            for (int32 Index = 0; Index < Setup->AggGeom.BoxElems.Num(); ++Index)
            {
                const FKBoxElem& Box = Setup->AggGeom.BoxElems[Index];
                PDI->SetHitProxy(new HPABTPrimitiveProxy(Setup->BoneName, EPABTViewportPrimitiveType::Box, Index));
                const FTransform ShapeTM = Box.GetTransform() * BoneTM;
                DrawWireBox(PDI, ShapeTM.ToMatrixWithScale(), FBox(FVector(-Box.X, -Box.Y, -Box.Z) * 0.5f, FVector(Box.X, Box.Y, Box.Z) * 0.5f), PrimitiveColor(EPABTViewportPrimitiveType::Box, Index), PrimitiveDepth(EPABTViewportPrimitiveType::Box, Index));
            }
            for (int32 Index = 0; Index < Setup->AggGeom.SphereElems.Num(); ++Index)
            {
                const FKSphereElem& Sphere = Setup->AggGeom.SphereElems[Index];
                PDI->SetHitProxy(new HPABTPrimitiveProxy(Setup->BoneName, EPABTViewportPrimitiveType::Sphere, Index));
                const FTransform ShapeTM = Sphere.GetTransform() * BoneTM;
                DrawWireSphere(PDI, ShapeTM.GetLocation(), PrimitiveColor(EPABTViewportPrimitiveType::Sphere, Index), Sphere.Radius, 24, PrimitiveDepth(EPABTViewportPrimitiveType::Sphere, Index));
            }
            for (int32 Index = 0; Index < Setup->AggGeom.SphylElems.Num(); ++Index)
            {
                const FKSphylElem& Capsule = Setup->AggGeom.SphylElems[Index];
                PDI->SetHitProxy(new HPABTPrimitiveProxy(Setup->BoneName, EPABTViewportPrimitiveType::Capsule, Index));
                const FTransform ShapeTM = Capsule.GetTransform() * BoneTM;
                DrawWireCapsule(PDI, ShapeTM.GetLocation(), ShapeTM.GetUnitAxis(EAxis::X), ShapeTM.GetUnitAxis(EAxis::Y), ShapeTM.GetUnitAxis(EAxis::Z), PrimitiveColor(EPABTViewportPrimitiveType::Capsule, Index), Capsule.Radius, Capsule.Length * 0.5f, 16, PrimitiveDepth(EPABTViewportPrimitiveType::Capsule, Index));
            }
            for (int32 Index = 0; Index < Setup->AggGeom.ConvexElems.Num(); ++Index)
            {
                const FKConvexElem& Convex = Setup->AggGeom.ConvexElems[Index];
                PDI->SetHitProxy(new HPABTPrimitiveProxy(Setup->BoneName, EPABTViewportPrimitiveType::Convex, Index));
                const FTransform ShapeTM = Convex.GetTransform() * BoneTM;
                DrawWireBox(PDI, ShapeTM.ToMatrixWithScale(), Convex.ElemBox, PrimitiveColor(EPABTViewportPrimitiveType::Convex, Index), PrimitiveDepth(EPABTViewportPrimitiveType::Convex, Index));
            }

            PDI->SetHitProxy(nullptr);

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
                PDI->DrawLine(A, B, FLinearColor(1.f, 0.f, 1.f, 1.f), SDPG_Foreground, 2.f);
                DrawWireSphere(PDI, A, FColor(255, 0, 255), 4.f, 8, SDPG_Foreground);
                DrawWireSphere(PDI, B, FColor(255, 0, 255), 4.f, 8, SDPG_Foreground);
            }
        }
    }

    void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override
    {
        if (HitProxy && HitProxy->IsA(HPABTPrimitiveProxy::StaticGetType()))
        {
            HPABTPrimitiveProxy* PrimitiveProxy = static_cast<HPABTPrimitiveProxy*>(HitProxy);
            if (Owner)
            {
                Owner->SetSelectedPrimitive(PrimitiveProxy->BoneName, PrimitiveProxy->PrimitiveType, PrimitiveProxy->PrimitiveIndex);
            }
            return;
        }
        FEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);
    }


    bool InputKey(const FInputKeyEventArgs& EventArgs) override
    {
        if (EventArgs.Event == IE_Released && Owner && EventArgs.Key == EKeys::LeftMouseButton)
        {
            bHasLastMousePosition = false;
            Owner->FinalizeSelectedBodyPhysics();
            return true;
        }
        if (EventArgs.Event == IE_Pressed && Owner)
        {
            if (EventArgs.Key == EKeys::W)
            {
                Owner->SetWidgetMode(UE::Widget::WM_Translate);
                return true;
            }
            if (EventArgs.Key == EKeys::E)
            {
                Owner->SetWidgetMode(UE::Widget::WM_Rotate);
                return true;
            }
            if (EventArgs.Key == EKeys::R)
            {
                Owner->SetWidgetMode(UE::Widget::WM_Scale);
                return true;
            }
            if (EventArgs.Key == EKeys::F)
            {
                Owner->FocusPreview();
                return true;
            }
        }
        return FEditorViewportClient::InputKey(EventArgs);
    }

    bool InputWidgetDelta(FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale) override
    {
        if (!Owner || !InViewport || CurrentAxis == EAxisList::None || !InViewport->KeyState(EKeys::LeftMouseButton))
        {
            bHasLastMousePosition = false;
            return false;
        }

        FVector EffectiveDrag = Drag;
        if (Owner->GetActiveWidgetMode() == UE::Widget::WM_Translate)
        {
            const FIntPoint CurrentMousePosition(InViewport->GetMouseX(), InViewport->GetMouseY());
            if (!bHasLastMousePosition)
            {
                LastMousePosition = CurrentMousePosition;
                bHasLastMousePosition = true;
                return false;
            }

            const FVector2D MouseDelta(
                static_cast<float>(CurrentMousePosition.X - LastMousePosition.X),
                static_cast<float>(CurrentMousePosition.Y - LastMousePosition.Y));
            LastMousePosition = CurrentMousePosition;

            const FRotationMatrix ViewRotationMatrix(GetViewRotation());
            const FVector ViewRight = ViewRotationMatrix.GetScaledAxis(EAxis::Y);
            const FVector ViewUp = ViewRotationMatrix.GetScaledAxis(EAxis::Z);

            auto HasAxis = [CurrentAxis](EAxisList::Type Axis)
            {
                return (CurrentAxis & Axis) != EAxisList::None;
            };
            auto AccumulateAxisDrag = [&](const FVector& WorldAxis, FVector& InOutDrag)
            {
                FVector2D ScreenAxis(FVector::DotProduct(WorldAxis, ViewRight), -FVector::DotProduct(WorldAxis, ViewUp));
                if (!ScreenAxis.Normalize())
                {
                    return;
                }
                constexpr float ScreenDragSensitivity = 4.f;
                InOutDrag += WorldAxis * FVector2D::DotProduct(MouseDelta, ScreenAxis) * ScreenDragSensitivity;
            };

            EffectiveDrag = FVector::ZeroVector;
            USkeletalMeshComponent* Component = Owner->GetPreviewComponent();
            const int32 BoneIndex = Component ? Component->GetBoneIndex(Owner->GetSelectedBone()) : INDEX_NONE;
            const FTransform AxisTransform = BoneIndex != INDEX_NONE ? Component->GetBoneTransform(BoneIndex) : FTransform::Identity;
            if (HasAxis(EAxisList::X)) AccumulateAxisDrag(AxisTransform.GetUnitAxis(EAxis::X), EffectiveDrag);
            if (HasAxis(EAxisList::Y)) AccumulateAxisDrag(AxisTransform.GetUnitAxis(EAxis::Y), EffectiveDrag);
            if (HasAxis(EAxisList::Z)) AccumulateAxisDrag(AxisTransform.GetUnitAxis(EAxis::Z), EffectiveDrag);

            if (EffectiveDrag.IsNearlyZero())
            {
                EffectiveDrag = Drag;
            }
        }

        return Owner->ApplySelectedBodyDelta(EffectiveDrag, Rot, Scale, CurrentAxis);
    }

    FVector GetWidgetLocation() const override
    {
        if (!Owner || !Owner->GetPreviewComponent() || Owner->GetSelectedBone().IsNone())
        {
            return FVector::ZeroVector;
        }
        const int32 BoneIndex = Owner->GetPreviewComponent()->GetBoneIndex(Owner->GetSelectedBone());
        return BoneIndex != INDEX_NONE ? Owner->GetPreviewComponent()->GetBoneTransform(BoneIndex).GetLocation() : FVector::ZeroVector;
    }

    UE::Widget::EWidgetMode GetWidgetMode() const override
    {
        return Owner ? Owner->GetActiveWidgetMode() : UE::Widget::WM_None;
    }

    bool UsesTransformWidget() const
    {
        return Owner && !Owner->GetSelectedBone().IsNone();
    }

private:
    SPABTViewport* Owner = nullptr;
    FIntPoint LastMousePosition = FIntPoint::ZeroValue;
    bool bHasLastMousePosition = false;
};
}

void SPABTViewport::Construct(const FArguments& InArgs)
{
    OnPrimitiveSelected = InArgs._OnPrimitiveSelected;
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
    SelectedPrimitiveType = EPABTViewportPrimitiveType::None;
    SelectedPrimitiveIndex = INDEX_NONE;
    if (ViewportClient.IsValid())
    {
        ViewportClient->Invalidate();
    }
}

void SPABTViewport::SetSelectedPrimitive(FName InBoneName, EPABTViewportPrimitiveType InPrimitiveType, int32 InPrimitiveIndex)
{
    SelectedBone = InBoneName;
    SelectedPrimitiveType = InPrimitiveType;
    SelectedPrimitiveIndex = InPrimitiveIndex;
    OnPrimitiveSelected.ExecuteIfBound(InBoneName, InPrimitiveType, InPrimitiveIndex);
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
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("Move", "Move (W)")).ToolTipText(LOCTEXT("MoveTooltip", "Translate selected body (W), like the standard editor shortcut.")).OnClicked(this, &SPABTViewport::SetTranslateMode)]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("Rotate", "Rotate (E)")).ToolTipText(LOCTEXT("RotateTooltip", "Rotate selected body (E), like the standard editor shortcut.")).OnClicked(this, &SPABTViewport::SetRotateMode)]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("Scale", "Scale (R)")).ToolTipText(LOCTEXT("ScaleTooltip", "Scale selected body (R), like the standard editor shortcut.")).OnClicked(this, &SPABTViewport::SetScaleMode)]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("Focus", "Focus (F)")).ToolTipText(LOCTEXT("FocusTooltip", "Focus selected preview (F)." )).OnClicked(this, &SPABTViewport::FocusPreview)]
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

void SPABTViewport::SetWidgetMode(UE::Widget::EWidgetMode InWidgetMode)
{
    WidgetMode = InWidgetMode;
    if (ViewportClient.IsValid())
    {
        ViewportClient->Invalidate();
    }
}

FReply SPABTViewport::SetTranslateMode() { SetWidgetMode(UE::Widget::WM_Translate); return FReply::Handled(); }
FReply SPABTViewport::SetRotateMode() { SetWidgetMode(UE::Widget::WM_Rotate); return FReply::Handled(); }
FReply SPABTViewport::SetScaleMode() { SetWidgetMode(UE::Widget::WM_Scale); return FReply::Handled(); }

void SPABTViewport::SelectBoneFromViewport(FName InBoneName)
{
    SelectedBone = InBoneName;
    OnPrimitiveSelected.ExecuteIfBound(InBoneName, EPABTViewportPrimitiveType::None, INDEX_NONE);
    if (ViewportClient.IsValid())
    {
        ViewportClient->Invalidate();
    }
}

bool SPABTViewport::ApplySelectedBodyDelta(const FVector& WorldDrag, const FRotator& RotationDelta, const FVector& ScaleDelta, EAxisList::Type CurrentAxis)
{
    UPhysicsAsset* Asset = PhysicsAsset.Get();
    if (!Asset || !PreviewComponent || SelectedBone.IsNone())
    {
        return false;
    }

    USkeletalBodySetup* Setup = nullptr;
    for (USkeletalBodySetup* Candidate : Asset->SkeletalBodySetups)
    {
        if (Candidate && Candidate->BoneName == SelectedBone)
        {
            Setup = Candidate;
            break;
        }
    }
    if (!Setup)
    {
        return false;
    }

    const int32 BoneIndex = PreviewComponent->GetBoneIndex(SelectedBone);
    if (BoneIndex == INDEX_NONE)
    {
        return false;
    }

    Asset->Modify();
    Setup->Modify();
    const FTransform BoneTM = PreviewComponent->GetBoneTransform(BoneIndex);
    const bool bTranslateMode = WidgetMode == UE::Widget::WM_Translate;
    const bool bRotateMode = WidgetMode == UE::Widget::WM_Rotate;
    const bool bScaleMode = WidgetMode == UE::Widget::WM_Scale;

    auto HasAxis = [CurrentAxis](EAxisList::Type Axis)
    {
        return (CurrentAxis & Axis) != EAxisList::None;
    };

    FVector AxisMask(
        HasAxis(EAxisList::X) ? 1.f : 0.f,
        HasAxis(EAxisList::Y) ? 1.f : 0.f,
        HasAxis(EAxisList::Z) ? 1.f : 0.f);
    if (AxisMask.IsNearlyZero())
    {
        AxisMask = FVector::OneVector;
    }

    FVector WorldConstrainedDrag = FVector::ZeroVector;
    if (bTranslateMode)
    {
        // FEditorViewportClient already converts the active widget axis drag into a
        // world-space constrained delta. Applying an additional camera-dependent
        // projection here makes users drag exactly along the arrow and can flip
        // direction when the view changes. Keep the engine-provided constrained
        // delta and only dampen it for physics-body authoring precision.
        constexpr float TranslationSensitivity = 0.05f;
        WorldConstrainedDrag = WorldDrag * TranslationSensitivity;
    }

    const FVector LocalDrag = BoneTM.InverseTransformVectorNoScale(WorldConstrainedDrag);
    const FQuat LocalRot = bRotateMode ? BoneTM.InverseTransformRotation(RotationDelta.Quaternion()) : FQuat::Identity;

    FVector ScaleAxisDelta = FVector::ZeroVector;
    if (bScaleMode)
    {
        constexpr float ScaleSensitivity = 0.01f;
        ScaleAxisDelta = FVector(ScaleDelta.X * AxisMask.X, ScaleDelta.Y * AxisMask.Y, ScaleDelta.Z * AxisMask.Z) * ScaleSensitivity;
        if (ScaleAxisDelta.IsNearlyZero())
        {
            const float UniformDelta = ScaleDelta.GetAbsMax() * ScaleSensitivity;
            ScaleAxisDelta = AxisMask * UniformDelta;
        }
    }

    auto ApplyTransformDelta = [&](auto& Elem)
    {
        FTransform TM = Elem.GetTransform();
        TM.AddToTranslation(LocalDrag);
        TM.ConcatenateRotation(LocalRot);
        Elem.SetTransform(TM);
    };

    auto ShouldApplyPrimitive = [&](EPABTViewportPrimitiveType Type, int32 Index)
    {
        return SelectedPrimitiveType == EPABTViewportPrimitiveType::None ||
            (SelectedPrimitiveType == Type && SelectedPrimitiveIndex == Index);
    };

    for (int32 Index = 0; Index < Setup->AggGeom.BoxElems.Num(); ++Index)
    {
        if (!ShouldApplyPrimitive(EPABTViewportPrimitiveType::Box, Index)) continue;
        FKBoxElem& Elem = Setup->AggGeom.BoxElems[Index];
        ApplyTransformDelta(Elem);
        if (bScaleMode)
        {
            Elem.X = FMath::Max(0.1f, Elem.X * (1.f + ScaleAxisDelta.X));
            Elem.Y = FMath::Max(0.1f, Elem.Y * (1.f + ScaleAxisDelta.Y));
            Elem.Z = FMath::Max(0.1f, Elem.Z * (1.f + ScaleAxisDelta.Z));
        }
    }
    for (int32 Index = 0; Index < Setup->AggGeom.SphereElems.Num(); ++Index)
    {
        if (!ShouldApplyPrimitive(EPABTViewportPrimitiveType::Sphere, Index)) continue;
        FKSphereElem& Elem = Setup->AggGeom.SphereElems[Index];
        ApplyTransformDelta(Elem);
        if (bScaleMode)
        {
            Elem.Radius = FMath::Max(0.1f, Elem.Radius * (1.f + ScaleAxisDelta.GetAbsMax()));
        }
    }
    for (int32 Index = 0; Index < Setup->AggGeom.SphylElems.Num(); ++Index)
    {
        if (!ShouldApplyPrimitive(EPABTViewportPrimitiveType::Capsule, Index)) continue;
        FKSphylElem& Elem = Setup->AggGeom.SphylElems[Index];
        ApplyTransformDelta(Elem);
        if (bScaleMode)
        {
            Elem.Radius = FMath::Max(0.1f, Elem.Radius * (1.f + FMath::Max(FMath::Abs(ScaleAxisDelta.X), FMath::Abs(ScaleAxisDelta.Y))));
            Elem.Length = FMath::Max(0.1f, Elem.Length * (1.f + ScaleAxisDelta.Z));
        }
    }
    for (int32 Index = 0; Index < Setup->AggGeom.ConvexElems.Num(); ++Index)
    {
        if (!ShouldApplyPrimitive(EPABTViewportPrimitiveType::Convex, Index)) continue;
        FKConvexElem& Elem = Setup->AggGeom.ConvexElems[Index];
        ApplyTransformDelta(Elem);
        if (bScaleMode)
        {
            FTransform TM = Elem.GetTransform();
            TM.SetScale3D(TM.GetScale3D() * (FVector::OneVector + ScaleAxisDelta));
            Elem.SetTransform(TM);
        }
        Elem.UpdateElemBox();
    }

    Asset->MarkPackageDirty();
    if (ViewportClient.IsValid())
    {
        ViewportClient->Invalidate();
    }
    return true;
}

void SPABTViewport::FinalizeSelectedBodyPhysics()
{
    UPhysicsAsset* Asset = PhysicsAsset.Get();
    if (!Asset || SelectedBone.IsNone())
    {
        return;
    }

    for (USkeletalBodySetup* Setup : Asset->SkeletalBodySetups)
    {
        if (Setup && Setup->BoneName == SelectedBone)
        {
            Setup->Modify();
            Setup->InvalidatePhysicsData();
            Setup->CreatePhysicsMeshes();
            Asset->UpdateBodySetupIndexMap();
            Asset->MarkPackageDirty();
            break;
        }
    }
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
