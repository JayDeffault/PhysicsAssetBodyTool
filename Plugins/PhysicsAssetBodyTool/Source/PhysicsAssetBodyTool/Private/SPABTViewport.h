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
    TSharedPtr<FAdvancedPreviewScene> PreviewScene;
    TSharedPtr<FEditorViewportClient> ViewportClient;
    USkeletalMeshComponent* PreviewComponent = nullptr;
    TWeakObjectPtr<UPhysicsAsset> PhysicsAsset;
    FName SelectedBone;
};
