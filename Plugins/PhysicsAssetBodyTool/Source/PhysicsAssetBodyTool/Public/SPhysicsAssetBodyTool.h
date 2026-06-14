#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "PhysicsAssetBodyToolSubsystems.h"

class IDetailsView;
class SPABTViewport;
class SSearchBox;
class UPhysicsAsset;
class USkeletalMesh;
class USkeletalBodySetup;

struct FPABTBoneItem : public TSharedFromThis<FPABTBoneItem>
{
    FName BoneName; int32 BoneIndex = INDEX_NONE; TArray<TSharedPtr<FPABTBoneItem>> Children;
};

struct FPABTBodyTreeItem : public TSharedFromThis<FPABTBodyTreeItem>
{
    enum class EKind : uint8 { Body, Primitive, ConstraintGroup, Constraint };
    EKind Kind = EKind::Body;
    FName BoneName;
    FText Label;
    TArray<TSharedPtr<FPABTBodyTreeItem>> Children;
};

class SPhysicsAssetBodyTool : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPhysicsAssetBodyTool){} SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
private:
    void SetSkeletalMesh(const FAssetData& Data);
    void SetPhysicsAsset(const FAssetData& Data);
    void RebuildBoneTree();
    void RebuildPhysicsTree();
    void RefreshLists();
    void RefreshPreviewAndDetails();
    TSharedRef<ITableRow> MakeBoneRow(TSharedPtr<FPABTBoneItem> Item, const TSharedRef<STableViewBase>& Owner);
    TSharedRef<ITableRow> MakeBodyTreeRow(TSharedPtr<FPABTBodyTreeItem> Item, const TSharedRef<STableViewBase>& Owner);
    void OnBodyTreeSelectionChanged(TSharedPtr<FPABTBodyTreeItem> Item, ESelectInfo::Type SelectInfo);
    void OnBoneSelectionChanged(TSharedPtr<FPABTBoneItem> Item, ESelectInfo::Type SelectInfo);
    void OnViewportBoneSelected(FName BoneName);
    TSharedPtr<SWidget> BuildBoneContextMenu();
    void SetSelectedBodiesPhysicsType(EPhysicsType NewPhysicsType);
    TSharedRef<SWidget> BuildAssetBar();
    TSharedRef<SWidget> BuildBodyPanel();
    TSharedRef<SWidget> BuildConstraintPanel();
    TSharedRef<SWidget> BuildValidationPanel();
    FReply AddPrimitive(EPABTPrimitiveType Type);
    FReply DeleteSelectedBody();
    FReply MirrorSelectedBody();
    FReply CreateDoorConstraint();
    FReply RunValidation();
    bool BoneFilter(TSharedPtr<FPABTBoneItem> Item) const;
    USkeletalMesh* SkeletalMesh = nullptr;
    UPhysicsAsset* PhysicsAsset = nullptr;
    FName SelectedBone;
    TArray<TSharedPtr<FPABTBoneItem>> RootBones;
    TArray<TSharedPtr<FPABTBoneItem>> VisibleRootBones;
    TArray<TSharedPtr<FPABTBodyTreeItem>> BodyTreeRoots;
    TArray<FPABTValidationIssue> Issues;
    TSharedPtr<STreeView<TSharedPtr<FPABTBoneItem>>> BoneTree;
    TSharedPtr<STreeView<TSharedPtr<FPABTBodyTreeItem>>> BodyTree;
    TSharedPtr<SVerticalBox> BodyList;
    TSharedPtr<SVerticalBox> ConstraintList;
    TSharedPtr<SVerticalBox> ValidationList;
    TSharedPtr<IDetailsView> DetailsView;
    TSharedPtr<SPABTViewport> ViewportWidget;
    TSharedPtr<SSearchBox> SearchBox;
    FString SearchText;
    FPABTMirrorSystem MirrorSystem;
};
