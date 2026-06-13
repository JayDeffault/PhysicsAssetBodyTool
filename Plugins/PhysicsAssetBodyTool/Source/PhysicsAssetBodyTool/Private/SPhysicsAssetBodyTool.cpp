#include "SPhysicsAssetBodyTool.h"
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
#include "Engine/SkeletalMesh.h"

#define LOCTEXT_NAMESPACE "PhysicsAssetBodyTool"

void SPhysicsAssetBodyTool::Construct(const FArguments& InArgs)
{
    ChildSlot [ SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[BuildAssetBar()]
        + SVerticalBox::Slot().FillHeight(1.f)[ SNew(SSplitter)
            + SSplitter::Slot().Value(.25f)[ SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[ SAssignNew(SearchBox, SSearchBox).OnTextChanged_Lambda([this](const FText& T){ SearchText=T.ToString(); RebuildBoneTree(); }) ]
                + SVerticalBox::Slot().FillHeight(1.f)[ SAssignNew(BoneTree, STreeView<TSharedPtr<FPABTBoneItem>>).TreeItemsSource(&VisibleRootBones).SelectionMode(ESelectionMode::Multi).OnGenerateRow(this,&SPhysicsAssetBodyTool::MakeBoneRow).OnGetChildren_Lambda([](TSharedPtr<FPABTBoneItem> I,TArray<TSharedPtr<FPABTBoneItem>>& C){ C=I->Children; }).OnSelectionChanged(this,&SPhysicsAssetBodyTool::OnBoneSelectionChanged) ]]
            + SSplitter::Slot().Value(.25f)[BuildBodyPanel()]
            + SSplitter::Slot().Value(.25f)[BuildConstraintPanel()]
            + SSplitter::Slot().Value(.25f)[BuildValidationPanel()]
        ]
    ];
}

TSharedRef<SWidget> SPhysicsAssetBodyTool::BuildAssetBar()
{
    FAssetPickerConfig MeshCfg; MeshCfg.Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName()); MeshCfg.OnAssetSelected = FOnAssetSelected::CreateSP(this,&SPhysicsAssetBodyTool::SetSkeletalMesh);
    FAssetPickerConfig PhysCfg; PhysCfg.Filter.ClassPaths.Add(UPhysicsAsset::StaticClass()->GetClassPathName()); PhysCfg.OnAssetSelected = FOnAssetSelected::CreateSP(this,&SPhysicsAssetBodyTool::SetPhysicsAsset);
    FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(.5f).MaxWidth(420)[SNew(SBox).HeightOverride(96)[CB.Get().CreateAssetPicker(MeshCfg)]]
        + SHorizontalBox::Slot().FillWidth(.5f).MaxWidth(420)[SNew(SBox).HeightOverride(96)[CB.Get().CreateAssetPicker(PhysCfg)]];
}

void SPhysicsAssetBodyTool::SetSkeletalMesh(const FAssetData& Data)
{
    SkeletalMesh = Cast<USkeletalMesh>(Data.GetAsset());
    if (SkeletalMesh && SkeletalMesh->GetPhysicsAsset()) PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
    RebuildBoneTree(); RefreshLists();
}
void SPhysicsAssetBodyTool::SetPhysicsAsset(const FAssetData& Data) { PhysicsAsset = Cast<UPhysicsAsset>(Data.GetAsset()); RefreshLists(); }

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

TSharedRef<ITableRow> SPhysicsAssetBodyTool::MakeBoneRow(TSharedPtr<FPABTBoneItem> Item, const TSharedRef<STableViewBase>& Owner)
{
    const bool bHasBody = PhysicsAsset && FPABTAssetEditor::FindBody(PhysicsAsset, Item->BoneName);
    FName Mirror; const bool bMirror = MirrorSystem.FindMirrorName(Item->BoneName, Mirror);
    return SNew(STableRow<TSharedPtr<FPABTBoneItem>>, Owner)[ SNew(STextBlock).Text(FText::FromName(Item->BoneName)).ColorAndOpacity(bHasBody ? FLinearColor::Green : (bMirror ? FLinearColor(.45f,.65f,1.f) : FLinearColor::White)) ];
}
void SPhysicsAssetBodyTool::OnBoneSelectionChanged(TSharedPtr<FPABTBoneItem> Item, ESelectInfo::Type) { if (Item) SelectedBone = Item->BoneName; RefreshLists(); }

TSharedRef<SWidget> SPhysicsAssetBodyTool::BuildBodyPanel()
{
    return SNew(SScrollBox)+SScrollBox::Slot()[ SAssignNew(BodyList,SVerticalBox) ];
}
TSharedRef<SWidget> SPhysicsAssetBodyTool::BuildConstraintPanel()
{
    return SNew(SScrollBox)+SScrollBox::Slot()[ SAssignNew(ConstraintList,SVerticalBox) ];
}
TSharedRef<SWidget> SPhysicsAssetBodyTool::BuildValidationPanel()
{
    return SNew(SScrollBox)+SScrollBox::Slot()[ SAssignNew(ValidationList,SVerticalBox) ];
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

FReply SPhysicsAssetBodyTool::AddPrimitive(EPABTPrimitiveType Type) { if (PhysicsAsset && !SelectedBone.IsNone()) FPABTAssetEditor::AddPrimitive(PhysicsAsset, SelectedBone, Type, 25.f); RefreshLists(); return FReply::Handled(); }
FReply SPhysicsAssetBodyTool::DeleteSelectedBody() { FPABTAssetEditor::DeleteBody(PhysicsAsset, SelectedBone); RefreshLists(); RebuildBoneTree(); return FReply::Handled(); }
FReply SPhysicsAssetBodyTool::MirrorSelectedBody() { FName M; if (MirrorSystem.FindMirrorName(SelectedBone, M)) MirrorSystem.MirrorBody(PhysicsAsset, SkeletalMesh, SelectedBone, M, EPABTMirrorAxis::X, false); RefreshLists(); RebuildBoneTree(); return FReply::Handled(); }
FReply SPhysicsAssetBodyTool::CreateDoorConstraint() { if (SkeletalMesh && PhysicsAsset) { int32 I=SkeletalMesh->GetRefSkeleton().FindBoneIndex(SelectedBone); int32 P= I!=INDEX_NONE ? SkeletalMesh->GetRefSkeleton().GetParentIndex(I) : INDEX_NONE; if (P!=INDEX_NONE) FPABTConstraintSystem::CreateConstraint(PhysicsAsset, SkeletalMesh->GetRefSkeleton().GetBoneName(P), SelectedBone, EPABTHingePreset::VehicleDoor, FVector::UpVector, 0, 70); } RefreshLists(); return FReply::Handled(); }
FReply SPhysicsAssetBodyTool::RunValidation() { Issues = FPABTValidationSystem::Validate(PhysicsAsset, SkeletalMesh, MirrorSystem); RefreshLists(); return FReply::Handled(); }

#undef LOCTEXT_NAMESPACE
