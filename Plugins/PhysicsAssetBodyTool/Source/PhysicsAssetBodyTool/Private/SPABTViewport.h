#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"
#include "UnrealWidget.h"

class FAdvancedPreviewScene;
class FEditorViewportClient;
class FPrimitiveDrawInterface;
class UPhysicsAsset;
class USkeletalMesh;
class USkeletalMeshComponent;

enum class EPABTViewportPrimitiveType : uint8 { None, Box, Sphere, Capsule, Convex };

DECLARE_DELEGATE_ThreeParams(FPABTOnViewportPrimitiveSelected, FName, EPABTViewportPrimitiveType, int32);

class SPABTViewport : public SEditorViewport
{
public:
    SLATE_BEGIN_ARGS(SPABTViewport) {}
        SLATE_EVENT(FPABTOnViewportPrimitiveSelected, OnPrimitiveSelected)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void SetPreviewAssets(USkeletalMesh* InSkeletalMesh, UPhysicsAsset* InPhysicsAsset);
    void SetSelectedBone(FName InBoneName);
    void SetSelectedPrimitive(FName InBoneName, EPABTViewportPrimitiveType InPrimitiveType, int32 InPrimitiveIndex);
    void SelectBoneFromViewport(FName InBoneName);
    bool ApplySelectedBodyDelta(const FVector& WorldDrag, const FRotator& RotationDelta, const FVector& ScaleDelta, EAxisList::Type CurrentAxis);
    void FinalizeSelectedBodyPhysics();
    FReply FocusPreview();
    void SetWidgetMode(UE::Widget::EWidgetMode InWidgetMode);
    USkeletalMeshComponent* GetPreviewComponent() const { return PreviewComponent; }
    UPhysicsAsset* GetPhysicsAsset() const { return PhysicsAsset.Get(); }
    FName GetSelectedBone() const { return SelectedBone; }
    EPABTViewportPrimitiveType GetSelectedPrimitiveType() const { return SelectedPrimitiveType; }
    int32 GetSelectedPrimitiveIndex() const { return SelectedPrimitiveIndex; }
    UE::Widget::EWidgetMode GetActiveWidgetMode() const { return WidgetMode; }
    FVector GetSelectedWidgetLocation() const;

protected:
    TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
    TSharedPtr<SWidget> MakeViewportToolbar() override;

private:
    void ApplyShowFlags();
    FReply ToggleBodies();
    FReply ToggleBones();
    FReply ToggleFloor();
    FReply ToggleGrid();
    FReply SetTranslateMode();
    FReply SetRotateMode();
    FReply SetScaleMode();
    FText GetStatsText() const;

    TSharedPtr<FAdvancedPreviewScene> PreviewScene;
    TSharedPtr<FEditorViewportClient> ViewportClient;
    USkeletalMeshComponent* PreviewComponent = nullptr;
    TWeakObjectPtr<UPhysicsAsset> PhysicsAsset;
    FName SelectedBone;
    EPABTViewportPrimitiveType SelectedPrimitiveType = EPABTViewportPrimitiveType::None;
    int32 SelectedPrimitiveIndex = INDEX_NONE;
    bool bShowBodies = true;
    bool bShowBones = false;
    bool bShowFloor = false;
    bool bShowGrid = true;
    FPABTOnViewportPrimitiveSelected OnPrimitiveSelected;
    UE::Widget::EWidgetMode WidgetMode = UE::Widget::WM_Translate;
};
