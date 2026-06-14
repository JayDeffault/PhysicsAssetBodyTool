#include "SPABTViewport.h"

#include "AdvancedPreviewScene.h"
#include "EditorViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Widgets/SNullWidget.h"

void SPABTViewport::Construct(const FArguments& InArgs)
{
    PreviewScene = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());
    PreviewComponent = NewObject<USkeletalMeshComponent>();
    PreviewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
    }
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
    ViewportClient->EngineShowFlags.SetGrid(true);
    ViewportClient->EngineShowFlags.SetSelectionOutline(true);
    return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SPABTViewport::MakeViewportToolbar()
{
    return SNullWidget::NullWidget;
}
