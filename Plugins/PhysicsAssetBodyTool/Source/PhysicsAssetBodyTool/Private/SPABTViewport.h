#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"

class FAdvancedPreviewScene;
class FEditorViewportClient;
class UPhysicsAsset;
class USkeletalMesh;
class USkeletalMeshComponent;

class SPABTViewport : public SEditorViewport
{
public:
    SLATE_BEGIN_ARGS(SPABTViewport) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void SetPreviewAssets(USkeletalMesh* InSkeletalMesh, UPhysicsAsset* InPhysicsAsset);
    void SetSelectedBone(FName InBoneName);

protected:
    TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
    TSharedPtr<SWidget> MakeViewportToolbar() override;

private:
    void ApplyShowFlags();
    FReply ToggleBodies();
    FReply ToggleBones();
    FReply ToggleFloor();
    FReply ToggleGrid();
    FReply FocusPreview();
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
};
