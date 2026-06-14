#include "SPhysicsAssetBodyTool.h"
#include "SPABTViewport.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Engine/SkeletalMesh.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "PhysicsAssetBodyTool"

void SPhysicsAssetBodyTool::Construct(const FArguments& InArgs)
{
    FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    FDetailsViewArgs DetailsArgs;
    DetailsArgs.bHideSelectionTip = true;
    DetailsArgs.bAllowSearch = true;
    DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
    DetailsView = PropertyEditor.CreateDetailView(DetailsArgs);

    ChildSlot [ SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[BuildAssetBar()]
        + SVerticalBox::Slot().AutoHeight()[
            SNew(SBorder)
            .Padding(FMargin(8.f, 4.f))
            [SNew(STextBlock).Text(this, &SPhysicsAssetBodyTool::GetSelectionSummaryText)]
        ]
        + SVerticalBox::Slot().FillHeight(1.f)[ SNew(SSplitter)
            + SSplitter::Slot().Value(.28f)[ SNew(SSplitter).Orientation(Orient_Vertical)
                + SSplitter::Slot().Value(.52f)[ SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(LOCTEXT("SkeletonTreeHeader", "Skeleton")) ]
                    + SVerticalBox::Slot().AutoHeight()[ SAssignNew(SearchBox, SSearchBox).HintText(LOCTEXT("SearchBones", "Search bones")).OnTextChanged_Lambda([this](const FText& T){ SearchText=T.ToString(); RebuildBoneTree(); }) ]
                    + SVerticalBox::Slot().FillHeight(1.f)[ SAssignNew(BoneTree, STreeView<TSharedPtr<FPABTBoneItem>>).TreeItemsSource(&VisibleRootBones).SelectionMode(ESelectionMode::Multi).OnGenerateRow(this,&SPhysicsAssetBodyTool::MakeBoneRow).OnGetChildren_Lambda([](TSharedPtr<FPABTBoneItem> I,TArray<TSharedPtr<FPABTBoneItem>>& C){ C=I->Children; }).OnSelectionChanged(this,&SPhysicsAssetBodyTool::OnBoneSelectionChanged).OnContextMenuOpening(this,&SPhysicsAssetBodyTool::BuildBoneContextMenu) ]]
                + SSplitter::Slot().Value(.48f)[ SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(LOCTEXT("PhysicsTreeHeader", "Physics Bodies / Primitives")) ]
                    + SVerticalBox::Slot().FillHeight(1.f)[ SAssignNew(BodyTree, STreeView<TSharedPtr<FPABTBodyTreeItem>>).TreeItemsSource(&BodyTreeRoots).SelectionMode(ESelectionMode::Single).OnGenerateRow(this,&SPhysicsAssetBodyTool::MakeBodyTreeRow).OnGetChildren_Lambda([](TSharedPtr<FPABTBodyTreeItem> I,TArray<TSharedPtr<FPABTBodyTreeItem>>& C){ C=I->Children; }).OnSelectionChanged(this,&SPhysicsAssetBodyTool::OnBodyTreeSelectionChanged) ]]
            ]
            + SSplitter::Slot().Value(.47f)[ SNew(SSplitter).Orientation(Orient_Vertical)
                + SSplitter::Slot().Value(.62f)[ SAssignNew(ViewportWidget, SPABTViewport).OnPrimitiveSelected(this, &SPhysicsAssetBodyTool::OnViewportPrimitiveSelected) ]
                + SSplitter::Slot().Value(.38f)[ DetailsView.ToSharedRef() ] ]
            + SSplitter::Slot().Value(.25f)[ SNew(SSplitter).Orientation(Orient_Vertical)
                + SSplitter::Slot().Value(.38f)[BuildBodyPanel()]
                + SSplitter::Slot().Value(.32f)[BuildConstraintPanel()]
                + SSplitter::Slot().Value(.30f)[BuildValidationPanel()] ]
        ]
    ];
}

TSharedRef<SWidget> SPhysicsAssetBodyTool::BuildAssetBar()
{
    FAssetPickerConfig MeshCfg; MeshCfg.Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName()); MeshCfg.OnAssetSelected = FOnAssetSelected::CreateSP(this,&SPhysicsAssetBodyTool::SetSkeletalMesh);
    FAssetPickerConfig PhysCfg; PhysCfg.Filter.ClassPaths.Add(UPhysicsAsset::StaticClass()->GetClassPathName()); PhysCfg.OnAssetSelected = FOnAssetSelected::CreateSP(this,&SPhysicsAssetBodyTool::SetPhysicsAsset);
    FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    return SNew(SExpandableArea)
        .InitiallyCollapsed(false)
        .HeaderContent()[SNew(STextBlock).Text(LOCTEXT("AssetSelectionHeader", "Asset Selection (collapse after selecting assets)"))]
        .BodyContent()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(.5f).MaxWidth(420)[SNew(SBox).HeightOverride(72)[CB.Get().CreateAssetPicker(MeshCfg)]]
            + SHorizontalBox::Slot().FillWidth(.5f).MaxWidth(420)[SNew(SBox).HeightOverride(72)[CB.Get().CreateAssetPicker(PhysCfg)]]
        ];
}

void SPhysicsAssetBodyTool::SetSkeletalMesh(const FAssetData& Data)
{
    SkeletalMesh = Cast<USkeletalMesh>(Data.GetAsset());
    if (SkeletalMesh && SkeletalMesh->GetPhysicsAsset()) PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
    RebuildBoneTree(); RebuildPhysicsTree(); RefreshPreviewAndDetails(); RefreshLists();
}
void SPhysicsAssetBodyTool::SetPhysicsAsset(const FAssetData& Data) { PhysicsAsset = Cast<UPhysicsAsset>(Data.GetAsset()); RebuildPhysicsTree(); RefreshPreviewAndDetails(); RefreshLists(); }

void SPhysicsAssetBodyTool::RebuildBoneTree()
{
    RootBones.Reset(); VisibleRootBones.Reset(); if (!SkeletalMesh) { if (BoneTree) BoneTree->RequestTreeRefresh(); return; }
    const FReferenceSkeleton& Ref = SkeletalMesh->GetRefSkeleton(); TArray<TSharedPtr<FPABTBoneItem>> All; All.SetNum(Ref.GetNum());
    for (int32 I=0; I<Ref.GetNum(); ++I) { All[I]=MakeShared<FPABTBoneItem>(); All[I]->BoneName=Ref.GetBoneName(I); All[I]->BoneIndex=I; }
    for (int32 I=0; I<Ref.GetNum(); ++I) { int32 P=Ref.GetParentIndex(I); if (P==INDEX_NONE) RootBones.Add(All[I]); else All[P]->Children.Add(All[I]); }
    for (auto& R: RootBones) if (BoneFilter(R)) VisibleRootBones.Add(R);
    if (BoneTree) BoneTree->RequestTreeRefresh();
}

bool SPhysicsAssetBodyTool::BoneFilter(TSharedPtr<FPABTBoneItem> Item) const
{
    return SearchText.IsEmpty() || Item->BoneName.ToString().Contains(SearchText);
}

FText SPhysicsAssetBodyTool::GetSelectionSummaryText() const
{
    const FText MeshName = SkeletalMesh ? FText::FromString(SkeletalMesh->GetName()) : LOCTEXT("NoMeshSelected", "No Skeletal Mesh");
    const FText AssetName = PhysicsAsset ? FText::FromString(PhysicsAsset->GetName()) : LOCTEXT("NoPhysicsAssetSelected", "No Physics Asset");
    const FText BoneName = SelectedBone.IsNone() ? LOCTEXT("NoBodySelected", "No body selected") : FText::FromName(SelectedBone);
    return FText::Format(
        LOCTEXT("SelectionSummary", "Mesh: {0}  |  Physics Asset: {1}  |  Selected: {2}  |  Workflow: click primitive, use W/E/R gizmo modes, F focus, RMB in skeleton for Simulated/Kinematic"),
        MeshName, AssetName, BoneName);
}


void SPhysicsAssetBodyTool::RebuildPhysicsTree()
{
    BodyTreeRoots.Reset();
    if (!PhysicsAsset)
    {
        if (BodyTree) BodyTree->RequestTreeRefresh();
        return;
    }

    for (USkeletalBodySetup* Setup : PhysicsAsset->SkeletalBodySetups)
    {
        if (!Setup) continue;
        TSharedPtr<FPABTBodyTreeItem> BodyItem = MakeShared<FPABTBodyTreeItem>();
        BodyItem->Kind = FPABTBodyTreeItem::EKind::Body;
        BodyItem->BoneName = Setup->BoneName;
        BodyItem->Label = FText::Format(LOCTEXT("BodyTreeBody", "[Body] {0}"), FText::FromName(Setup->BoneName));

        auto AddPrimitiveChild = [&](const FText& Label, EPABTPrimitiveType PrimitiveType, int32 PrimitiveIndex)
        {
            TSharedPtr<FPABTBodyTreeItem> Child = MakeShared<FPABTBodyTreeItem>();
            Child->Kind = FPABTBodyTreeItem::EKind::Primitive;
            Child->BoneName = Setup->BoneName;
            Child->Label = Label;
            Child->PrimitiveType = PrimitiveType;
            Child->PrimitiveIndex = PrimitiveIndex;
            BodyItem->Children.Add(Child);
        };
        for (int32 Index = 0; Index < Setup->AggGeom.BoxElems.Num(); ++Index) AddPrimitiveChild(FText::Format(LOCTEXT("BodyTreeBox", "Box {0}"), Index), EPABTPrimitiveType::Box, Index);
        for (int32 Index = 0; Index < Setup->AggGeom.SphereElems.Num(); ++Index) AddPrimitiveChild(FText::Format(LOCTEXT("BodyTreeSphere", "Sphere {0}"), Index), EPABTPrimitiveType::Sphere, Index);
        for (int32 Index = 0; Index < Setup->AggGeom.SphylElems.Num(); ++Index) AddPrimitiveChild(FText::Format(LOCTEXT("BodyTreeCapsule", "Capsule {0}"), Index), EPABTPrimitiveType::Capsule, Index);
        for (int32 Index = 0; Index < Setup->AggGeom.ConvexElems.Num(); ++Index) AddPrimitiveChild(FText::Format(LOCTEXT("BodyTreeConvex", "Convex {0}"), Index), EPABTPrimitiveType::Convex, Index);
        BodyTreeRoots.Add(BodyItem);
    }

    TSharedPtr<FPABTBodyTreeItem> ConstraintRoot = MakeShared<FPABTBodyTreeItem>();
    ConstraintRoot->Kind = FPABTBodyTreeItem::EKind::ConstraintGroup;
    ConstraintRoot->Label = LOCTEXT("ConstraintsRoot", "Constraints");
    for (UPhysicsConstraintTemplate* Constraint : PhysicsAsset->ConstraintSetup)
    {
        if (!Constraint) continue;
        TSharedPtr<FPABTBodyTreeItem> Item = MakeShared<FPABTBodyTreeItem>();
        Item->Kind = FPABTBodyTreeItem::EKind::Constraint;
        Item->BoneName = Constraint->DefaultInstance.ConstraintBone2;
        Item->Label = FText::Format(LOCTEXT("ConstraintTreeItem", "{0} -> {1}"), FText::FromName(Constraint->DefaultInstance.ConstraintBone1), FText::FromName(Constraint->DefaultInstance.ConstraintBone2));
        ConstraintRoot->Children.Add(Item);
    }
    if (ConstraintRoot->Children.Num() > 0)
    {
        BodyTreeRoots.Add(ConstraintRoot);
    }

    if (BodyTree) BodyTree->RequestTreeRefresh();
}

TSharedRef<ITableRow> SPhysicsAssetBodyTool::MakeBodyTreeRow(TSharedPtr<FPABTBodyTreeItem> Item, const TSharedRef<STableViewBase>& Owner)
{
    const FLinearColor Color = Item->Kind == FPABTBodyTreeItem::EKind::Body ? FLinearColor::Green :
        Item->Kind == FPABTBodyTreeItem::EKind::Primitive ? FLinearColor(0.65f, 0.85f, 1.f) : FLinearColor(1.f, 0.45f, 1.f);
    return SNew(STableRow<TSharedPtr<FPABTBodyTreeItem>>, Owner)[SNew(STextBlock).Text(Item->Label).ColorAndOpacity(Color)];
}

void SPhysicsAssetBodyTool::OnBodyTreeSelectionChanged(TSharedPtr<FPABTBodyTreeItem> Item, ESelectInfo::Type)
{
    if (Item.IsValid() && !Item->BoneName.IsNone())
    {
        SelectedBone = Item->BoneName;
        RefreshPreviewAndDetails();
        if (ViewportWidget.IsValid())
        {
            const EPABTViewportPrimitiveType ViewportType = Item->Kind == FPABTBodyTreeItem::EKind::Primitive
                ? (Item->PrimitiveType == EPABTPrimitiveType::Box ? EPABTViewportPrimitiveType::Box : Item->PrimitiveType == EPABTPrimitiveType::Sphere ? EPABTViewportPrimitiveType::Sphere : Item->PrimitiveType == EPABTPrimitiveType::Capsule ? EPABTViewportPrimitiveType::Capsule : EPABTViewportPrimitiveType::Convex)
                : EPABTViewportPrimitiveType::None;
            ViewportWidget->SetSelectedPrimitive(Item->BoneName, ViewportType, Item->Kind == FPABTBodyTreeItem::EKind::Primitive ? Item->PrimitiveIndex : INDEX_NONE);
        }
        RefreshLists();
    }
}

TSharedPtr<SWidget> SPhysicsAssetBodyTool::BuildBoneContextMenu()
{
    FMenuBuilder MenuBuilder(true, nullptr);
    MenuBuilder.BeginSection("PhysicsBodyMode", LOCTEXT("PhysicsBodyMode", "Physics Body Mode"));
    MenuBuilder.AddMenuEntry(
        LOCTEXT("SetSimulated", "Convert Body to Simulated"),
        LOCTEXT("SetSimulatedTooltip", "Set the selected body's Physics Type to Simulated."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateSP(this, &SPhysicsAssetBodyTool::SetSelectedBodiesPhysicsType, PhysType_Simulated))
    );
    MenuBuilder.AddMenuEntry(
        LOCTEXT("SetKinematic", "Convert Body to Kinematic"),
        LOCTEXT("SetKinematicTooltip", "Set the selected body's Physics Type to Kinematic."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateSP(this, &SPhysicsAssetBodyTool::SetSelectedBodiesPhysicsType, PhysType_Kinematic))
    );
    MenuBuilder.EndSection();
    return MenuBuilder.MakeWidget();
}

void SPhysicsAssetBodyTool::SetSelectedBodiesPhysicsType(EPhysicsType NewPhysicsType)
{
    if (!PhysicsAsset || !BoneTree.IsValid())
    {
        return;
    }

    TArray<TSharedPtr<FPABTBoneItem>> SelectedItems = BoneTree->GetSelectedItems();
    if (SelectedItems.Num() == 0 && !SelectedBone.IsNone())
    {
        FPABTAssetEditor::SetBodyPhysicsType(PhysicsAsset, SelectedBone, NewPhysicsType);
    }
    else
    {
        for (const TSharedPtr<FPABTBoneItem>& Item : SelectedItems)
        {
            if (Item.IsValid())
            {
                FPABTAssetEditor::SetBodyPhysicsType(PhysicsAsset, Item->BoneName, NewPhysicsType);
            }
        }
    }
    RefreshPreviewAndDetails();
    RefreshLists();
    RebuildBoneTree();
}

TSharedRef<ITableRow> SPhysicsAssetBodyTool::MakeBoneRow(TSharedPtr<FPABTBoneItem> Item, const TSharedRef<STableViewBase>& Owner)
{
    USkeletalBodySetup* BodySetup = PhysicsAsset ? FPABTAssetEditor::FindBody(PhysicsAsset, Item->BoneName) : nullptr;
    const bool bHasBody = BodySetup != nullptr;
    const FString ModePrefix = BodySetup ? TEXT("[Body] ") : TEXT("       ");
    FName Mirror; const bool bMirror = MirrorSystem.FindMirrorName(Item->BoneName, Mirror);
    return SNew(STableRow<TSharedPtr<FPABTBoneItem>>, Owner)[ SNew(STextBlock).Text(FText::FromString(ModePrefix + Item->BoneName.ToString())).ColorAndOpacity(bHasBody ? FLinearColor::Green : (bMirror ? FLinearColor(.45f,.65f,1.f) : FLinearColor::White)) ];
}
void SPhysicsAssetBodyTool::OnBoneSelectionChanged(TSharedPtr<FPABTBoneItem> Item, ESelectInfo::Type) { if (Item) SelectedBone = Item->BoneName; RefreshPreviewAndDetails(); RefreshLists(); }
void SPhysicsAssetBodyTool::OnViewportPrimitiveSelected(FName BoneName, EPABTViewportPrimitiveType, int32) { SelectedBone = BoneName; RefreshPreviewAndDetails(); RefreshLists(); }

TSharedRef<SWidget> SPhysicsAssetBodyTool::BuildBodyPanel()
{
    return SNew(SExpandableArea)
        .InitiallyCollapsed(false)
        .HeaderContent()[SNew(STextBlock).Text(LOCTEXT("BodyEditorTab", "Body Editor"))]
        .BodyContent()[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(BodyList,SVerticalBox)]];
}
TSharedRef<SWidget> SPhysicsAssetBodyTool::BuildConstraintPanel()
{
    return SNew(SExpandableArea)
        .InitiallyCollapsed(false)
        .HeaderContent()[SNew(STextBlock).Text(LOCTEXT("ConstraintEditorTab", "Constraint Editor"))]
        .BodyContent()[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(ConstraintList,SVerticalBox)]];
}
TSharedRef<SWidget> SPhysicsAssetBodyTool::BuildValidationPanel()
{
    return SNew(SExpandableArea)
        .InitiallyCollapsed(false)
        .HeaderContent()[SNew(STextBlock).Text(LOCTEXT("ValidationTab", "Validation"))]
        .BodyContent()[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(ValidationList,SVerticalBox)]];
}


void SPhysicsAssetBodyTool::RefreshPreviewAndDetails()
{
    if (ViewportWidget.IsValid())
    {
        ViewportWidget->SetPreviewAssets(SkeletalMesh, PhysicsAsset);
        ViewportWidget->SetSelectedBone(SelectedBone);
    }

    if (DetailsView.IsValid())
    {
        UObject* ObjectToInspect = nullptr;
        if (PhysicsAsset && !SelectedBone.IsNone())
        {
            ObjectToInspect = FPABTAssetEditor::FindBody(PhysicsAsset, SelectedBone);
        }
        if (!ObjectToInspect)
        {
            ObjectToInspect = PhysicsAsset;
        }
        DetailsView->SetObject(ObjectToInspect);
    }
}

void SPhysicsAssetBodyTool::RefreshLists()
{
    if (BodyList)
    {
        BodyList->ClearChildren(); BodyList->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::Format(LOCTEXT("BodyTitle","Body Editor: {0}"), FText::FromName(SelectedBone)))];
        BodyList->AddSlot().AutoHeight()[SNew(SButton).Text(LOCTEXT("AddBox","Add Box")).OnClicked(this,&SPhysicsAssetBodyTool::AddPrimitive,EPABTPrimitiveType::Box)];
        BodyList->AddSlot().AutoHeight()[SNew(SButton).Text(LOCTEXT("AddSphere","Add Sphere")).OnClicked(this,&SPhysicsAssetBodyTool::AddPrimitive,EPABTPrimitiveType::Sphere)];
        BodyList->AddSlot().AutoHeight()[SNew(SButton).Text(LOCTEXT("AddCapsule","Add Capsule")).OnClicked(this,&SPhysicsAssetBodyTool::AddPrimitive,EPABTPrimitiveType::Capsule)];
        BodyList->AddSlot().AutoHeight()[SNew(SButton).Text(LOCTEXT("AddConvex","Add Convex Box Hull")).OnClicked(this,&SPhysicsAssetBodyTool::AddPrimitive,EPABTPrimitiveType::Convex)];
        BodyList->AddSlot().AutoHeight()[SNew(SButton).Text(LOCTEXT("MirrorBody","Mirror Body")).OnClicked(this,&SPhysicsAssetBodyTool::MirrorSelectedBody)];
        BodyList->AddSlot().AutoHeight()[SNew(SButton).Text(LOCTEXT("DeleteBody","Delete Body")).OnClicked(this,&SPhysicsAssetBodyTool::DeleteSelectedBody)];
        if (USkeletalBodySetup* S = FPABTAssetEditor::FindBody(PhysicsAsset, SelectedBone))
            BodyList->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::Format(LOCTEXT("PrimitiveCounts","Boxes {0} | Spheres {1} | Capsules {2} | Convex {3}"), S->AggGeom.BoxElems.Num(), S->AggGeom.SphereElems.Num(), S->AggGeom.SphylElems.Num(), S->AggGeom.ConvexElems.Num()))];
    }
    if (ConstraintList)
    {
        ConstraintList->ClearChildren(); ConstraintList->AddSlot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("ConstraintTitle","Constraint Editor"))];
        ConstraintList->AddSlot().AutoHeight()[SNew(SButton).Text(LOCTEXT("DoorConstraint","Create Vehicle Door Hinge To Parent")).OnClicked(this,&SPhysicsAssetBodyTool::CreateDoorConstraint)];
        if (PhysicsAsset) for (UPhysicsConstraintTemplate* C : PhysicsAsset->ConstraintSetup) if (C && (C->DefaultInstance.ConstraintBone1==SelectedBone || C->DefaultInstance.ConstraintBone2==SelectedBone)) ConstraintList->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::Format(LOCTEXT("ConstraintRow","{0} ↔ {1}"), FText::FromName(C->DefaultInstance.ConstraintBone1), FText::FromName(C->DefaultInstance.ConstraintBone2)))];
    }
    if (ValidationList) { ValidationList->ClearChildren(); ValidationList->AddSlot().AutoHeight()[SNew(SButton).Text(LOCTEXT("Validate","Validate Asset")).OnClicked(this,&SPhysicsAssetBodyTool::RunValidation)]; for (const FPABTValidationIssue& I: Issues) ValidationList->AddSlot().AutoHeight()[SNew(STextBlock).Text(I.Message).ColorAndOpacity(I.Severity==FPABTValidationIssue::ESeverity::Error?FLinearColor::Red:FLinearColor::Yellow)]; }
}

FReply SPhysicsAssetBodyTool::AddPrimitive(EPABTPrimitiveType Type) { if (PhysicsAsset && !SelectedBone.IsNone()) FPABTAssetEditor::AddPrimitive(PhysicsAsset, SelectedBone, Type, 25.f); RebuildPhysicsTree(); RefreshPreviewAndDetails(); RefreshLists(); return FReply::Handled(); }
FReply SPhysicsAssetBodyTool::DeleteSelectedBody() { FPABTAssetEditor::DeleteBody(PhysicsAsset, SelectedBone); RebuildPhysicsTree(); RefreshPreviewAndDetails(); RefreshLists(); RebuildBoneTree(); return FReply::Handled(); }
FReply SPhysicsAssetBodyTool::MirrorSelectedBody() { FName M; if (MirrorSystem.FindMirrorName(SelectedBone, M)) MirrorSystem.MirrorBody(PhysicsAsset, SkeletalMesh, SelectedBone, M, EPABTMirrorAxis::X, false); RebuildPhysicsTree(); RefreshPreviewAndDetails(); RefreshLists(); RebuildBoneTree(); return FReply::Handled(); }
FReply SPhysicsAssetBodyTool::CreateDoorConstraint() { if (SkeletalMesh && PhysicsAsset) { int32 I=SkeletalMesh->GetRefSkeleton().FindBoneIndex(SelectedBone); int32 P= I!=INDEX_NONE ? SkeletalMesh->GetRefSkeleton().GetParentIndex(I) : INDEX_NONE; if (P!=INDEX_NONE) FPABTConstraintSystem::CreateConstraint(PhysicsAsset, SkeletalMesh->GetRefSkeleton().GetBoneName(P), SelectedBone, EPABTHingePreset::VehicleDoor, FVector::UpVector, 0, 70); } RebuildPhysicsTree(); RefreshPreviewAndDetails(); RefreshLists(); return FReply::Handled(); }
FReply SPhysicsAssetBodyTool::RunValidation() { Issues = FPABTValidationSystem::Validate(PhysicsAsset, SkeletalMesh, MirrorSystem); RefreshLists(); return FReply::Handled(); }

#undef LOCTEXT_NAMESPACE
