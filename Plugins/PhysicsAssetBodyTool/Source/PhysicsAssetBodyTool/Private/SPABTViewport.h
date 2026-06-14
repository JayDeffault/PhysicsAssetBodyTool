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

DECLARE_DELEGATE_OneParam(FPABTOnViewportBoneSelected, FName);

class SPABTViewport : public SEditorViewport
{
public:
    SLATE_BEGIN_ARGS(SPABTViewport) {}
        SLATE_EVENT(FPABTOnViewportBoneSelected, OnBoneSelected)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void SetPreviewAssets(USkeletalMesh* InSkeletalMesh, UPhysicsAsset* InPhysicsAsset);
    void SetSelectedBone(FName InBoneName);
    void SelectBoneFromViewport(FName InBoneName);
    bool ApplySelectedBodyDelta(const FVector& WorldDrag, const FRotator& RotationDelta, const FVector& ScaleDelta, EAxisList::Type CurrentAxis);
    FReply FocusPreview();
    void SetWidgetMode(UE::Widget::EWidgetMode InWidgetMode);
    USkeletalMeshComponent* GetPreviewComponent() const { return PreviewComponent; }
    UPhysicsAsset* GetPhysicsAsset() const { return PhysicsAsset.Get(); }
    FName GetSelectedBone() const { return SelectedBone; }
    UE::Widget::EWidgetMode GetActiveWidgetMode() const { return WidgetMode; }

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
    bool bShowBodies = true;
    bool bShowBones = true;
    bool bShowFloor = true;
    bool bShowGrid = true;
    FPABTOnViewportBoneSelected OnBoneSelected;
    UE::Widget::EWidgetMode WidgetMode = UE::Widget::WM_Translate;
};
