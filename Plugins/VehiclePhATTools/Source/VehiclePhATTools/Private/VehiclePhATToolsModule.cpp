#include "VehiclePhATToolsModule.h"

#include "Framework/Application/SlateApplication.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"
#include "VehiclePhATBodyUtils.h"
#include "VehiclePhATClipboard.h"
#include "VehiclePhATConstraintUtils.h"
#include "VehiclePhATConvexUtils.h"
#include "VehiclePhATMirrorUtils.h"
#include "VehiclePhATToolsLog.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "VehiclePhATTools"

static const FName VehiclePhATToolsTabName(TEXT("VehiclePhATTools"));

namespace VehiclePhATToolsUI
{
static void ShowModalWindow(const FText& Title, const TSharedRef<SWidget>& Content, const FVector2D Size)
{
    const TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(Title)
        .ClientSize(Size)
        .SupportsMaximize(false)
        .SupportsMinimize(false)
        [
            Content
        ];

    FSlateApplication::Get().AddModalWindow(Window, nullptr);
}

static FName TextToName(const FText& Text)
{
    return FName(*Text.ToString().TrimStartAndEnd());
}
}

class SMirrorBodiesDialog final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMirrorBodiesDialog) {}
        SLATE_ARGUMENT(UPhysicsAsset*, PhysicsAsset)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        PhysicsAsset = InArgs._PhysicsAsset;
        RebuildPreview();

        ChildSlot
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("MirrorDialogHelp", "Preview mirrors bodies from source pattern to target pattern. Default is *l* -> *r*."))
                .AutoWrapText(true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel).SlotPadding(4)
                + SUniformGridPanel::Slot(0, 0)
                [SNew(STextBlock).Text(LOCTEXT("SourcePattern", "Source pattern"))]
                + SUniformGridPanel::Slot(1, 0)
                [SNew(SEditableTextBox).Text(this, &SMirrorBodiesDialog::GetSourcePatternText).OnTextCommitted(this, &SMirrorBodiesDialog::OnSourcePatternCommitted)]
                + SUniformGridPanel::Slot(0, 1)
                [SNew(STextBlock).Text(LOCTEXT("TargetPattern", "Target pattern"))]
                + SUniformGridPanel::Slot(1, 1)
                [SNew(SEditableTextBox).Text(this, &SMirrorBodiesDialog::GetTargetPatternText).OnTextCommitted(this, &SMirrorBodiesDialog::OnTargetPatternCommitted)]
                + SUniformGridPanel::Slot(0, 2)
                [SNew(STextBlock).Text(LOCTEXT("MirrorOptions", "Options"))]
                + SUniformGridPanel::Slot(1, 2)
                [SNew(STextBlock).Text(LOCTEXT("MirrorOptionsValue", "Axis: Y, Location+Rotation, Create missing, do not replace existing"))]
            ]
            + SVerticalBox::Slot().FillHeight(1.f).Padding(6)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()
                [SNew(STextBlock).Text(this, &SMirrorBodiesDialog::GetPreviewText).AutoWrapText(true)]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel).SlotPadding(4)
                + SUniformGridPanel::Slot(0, 0)
                [SNew(SButton).Text(LOCTEXT("PreviewMirror", "Preview")).OnClicked(this, &SMirrorBodiesDialog::OnPreview)]
                + SUniformGridPanel::Slot(1, 0)
                [SNew(SButton).Text(LOCTEXT("ApplyMirror", "Apply")).OnClicked(this, &SMirrorBodiesDialog::OnApply)]
            ]
        ];
    }

private:
    UPhysicsAsset* PhysicsAsset = nullptr;
    FVehiclePhATMirrorOptions Options;
    TArray<FVehiclePhATBodyPair> Pairs;
    FString PreviewText;

    FText GetSourcePatternText() const { return FText::FromString(Options.SourcePattern); }
    FText GetTargetPatternText() const { return FText::FromString(Options.TargetPattern); }
    FText GetPreviewText() const { return FText::FromString(PreviewText); }

    void OnSourcePatternCommitted(const FText& Text, ETextCommit::Type)
    {
        Options.SourcePattern = Text.ToString();
        RebuildPreview();
    }

    void OnTargetPatternCommitted(const FText& Text, ETextCommit::Type)
    {
        Options.TargetPattern = Text.ToString();
        RebuildPreview();
    }

    FReply OnPreview()
    {
        RebuildPreview();
        return FReply::Handled();
    }

    FReply OnApply()
    {
        RebuildPreview();
        FString Message;
        FVehiclePhATMirrorUtils::ApplyMirror(PhysicsAsset, Options, Pairs, Message);
        PreviewText += TEXT("\n\n") + Message;
        return FReply::Handled();
    }

    void RebuildPreview()
    {
        Pairs = FVehiclePhATMirrorUtils::BuildMirrorPairs(PhysicsAsset, Options);
        PreviewText = FString::Printf(TEXT("Found %d candidate pair(s):"), Pairs.Num());

        for (const FVehiclePhATBodyPair& Pair : Pairs)
        {
            PreviewText += FString::Printf(
                TEXT("\n[%s] %s -> %s | source body: %s | target bone: %s | target body: %s"),
                Pair.bSelected ? TEXT("x") : TEXT(" "),
                *Pair.SourceBone.ToString(),
                *Pair.TargetBone.ToString(),
                Pair.bSourceBodyExists ? TEXT("yes") : TEXT("no"),
                Pair.bTargetBoneExists ? TEXT("yes") : TEXT("no"),
                Pair.bTargetBodyExists ? TEXT("yes") : TEXT("no"));
        }
    }
};

class SCreateConstraintDialog final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCreateConstraintDialog) {}
        SLATE_ARGUMENT(UPhysicsAsset*, PhysicsAsset)
        SLATE_ARGUMENT(FName, DefaultParentBone)
        SLATE_ARGUMENT(FName, DefaultChildBone)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        PhysicsAsset = InArgs._PhysicsAsset;
        Options.ParentBone = InArgs._DefaultParentBone;
        Options.ChildBone = InArgs._DefaultChildBone;
        Options.Preset = EVehiclePhATConstraintPreset::Door70;
        Options.AngularLimitDegrees = 70.f;

        ChildSlot
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [SNew(STextBlock).Text(LOCTEXT("ConstraintHelp", "Create or update a simple vehicle hinge-style constraint."))]
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel).SlotPadding(4)
                + SUniformGridPanel::Slot(0, 0)
                [SNew(STextBlock).Text(LOCTEXT("ParentBone", "Parent body bone"))]
                + SUniformGridPanel::Slot(1, 0)
                [SNew(SEditableTextBox).Text(this, &SCreateConstraintDialog::GetParentBoneText).OnTextCommitted(this, &SCreateConstraintDialog::OnParentBoneCommitted)]
                + SUniformGridPanel::Slot(0, 1)
                [SNew(STextBlock).Text(LOCTEXT("ChildBone", "Child body bone"))]
                + SUniformGridPanel::Slot(1, 1)
                [SNew(SEditableTextBox).Text(this, &SCreateConstraintDialog::GetChildBoneText).OnTextCommitted(this, &SCreateConstraintDialog::OnChildBoneCommitted)]
                + SUniformGridPanel::Slot(0, 2)
                [SNew(STextBlock).Text(LOCTEXT("AngularLimit", "Angular limit degrees"))]
                + SUniformGridPanel::Slot(1, 2)
                [SNew(SNumericEntryBox<float>).Value(this, &SCreateConstraintDialog::GetAngularLimit).MinValue(0.f).MaxValue(180.f).OnValueChanged(this, &SCreateConstraintDialog::OnAngularLimitChanged)]
                + SUniformGridPanel::Slot(0, 3)
                [SNew(STextBlock).Text(LOCTEXT("UpdateExisting", "Update existing"))]
                + SUniformGridPanel::Slot(1, 3)
                [SNew(SCheckBox).IsChecked(this, &SCreateConstraintDialog::GetUpdateExistingState).OnCheckStateChanged(this, &SCreateConstraintDialog::OnUpdateExistingChanged)]
                + SUniformGridPanel::Slot(0, 4)
                [SNew(STextBlock).Text(LOCTEXT("FlipAxis", "Flip axis"))]
                + SUniformGridPanel::Slot(1, 4)
                [SNew(SCheckBox).IsChecked(this, &SCreateConstraintDialog::GetFlipAxisState).OnCheckStateChanged(this, &SCreateConstraintDialog::OnFlipAxisChanged)]
            ]
            + SVerticalBox::Slot().FillHeight(1.f).Padding(6)
            [SNew(STextBlock).Text(this, &SCreateConstraintDialog::GetStatusText).AutoWrapText(true)]
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [SNew(SButton).Text(LOCTEXT("ApplyConstraint", "Apply Constraint")).OnClicked(this, &SCreateConstraintDialog::OnApply)]
        ];
    }

private:
    UPhysicsAsset* PhysicsAsset = nullptr;
    FVehiclePhATConstraintOptions Options;
    FString Status;

    FText GetParentBoneText() const { return FText::FromName(Options.ParentBone); }
    FText GetChildBoneText() const { return FText::FromName(Options.ChildBone); }
    TOptional<float> GetAngularLimit() const { return Options.AngularLimitDegrees; }
    FText GetStatusText() const { return FText::FromString(Status); }
    ECheckBoxState GetUpdateExistingState() const { return Options.bUpdateExisting ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
    ECheckBoxState GetFlipAxisState() const { return Options.bFlipAxis ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }

    void OnParentBoneCommitted(const FText& Text, ETextCommit::Type) { Options.ParentBone = VehiclePhATToolsUI::TextToName(Text); }
    void OnChildBoneCommitted(const FText& Text, ETextCommit::Type) { Options.ChildBone = VehiclePhATToolsUI::TextToName(Text); }
    void OnAngularLimitChanged(float NewValue) { Options.AngularLimitDegrees = NewValue; Options.Preset = EVehiclePhATConstraintPreset::Custom; }
    void OnUpdateExistingChanged(ECheckBoxState State) { Options.bUpdateExisting = State == ECheckBoxState::Checked; }
    void OnFlipAxisChanged(ECheckBoxState State) { Options.bFlipAxis = State == ECheckBoxState::Checked; }

    FReply OnApply()
    {
        FString Message;
        FVehiclePhATConstraintUtils::CreateOrUpdateConstraint(PhysicsAsset, Options, Message);
        Status = Message;
        return FReply::Handled();
    }
};

class SVehiclePhATToolsPanel final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SVehiclePhATToolsPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments&)
    {
        RefreshSelection();

        ChildSlot
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(4)
                [SNew(STextBlock).Text(this, &SVehiclePhATToolsPanel::GetAssetText)]
                + SVerticalBox::Slot().AutoHeight().Padding(4)
                [SNew(SButton).Text(LOCTEXT("Refresh", "Refresh Selected PhysicsAsset")).OnClicked(this, &SVehiclePhATToolsPanel::OnRefresh)]
                + SVerticalBox::Slot().AutoHeight().Padding(4)
                [SNew(STextBlock).Text(this, &SVehiclePhATToolsPanel::GetBodiesText)]
                + SVerticalBox::Slot().AutoHeight().Padding(4)
                [SNew(SEditableTextBox).HintText(LOCTEXT("BoneHint", "Bone name for copy/paste/constraint child body")).Text(this, &SVehiclePhATToolsPanel::GetBoneText).OnTextCommitted(this, &SVehiclePhATToolsPanel::OnBoneCommitted)]
                + SVerticalBox::Slot().AutoHeight().Padding(4)
                [SNew(SEditableTextBox).HintText(LOCTEXT("ParentHint", "Parent body bone for constraint")).Text(this, &SVehiclePhATToolsPanel::GetParentText).OnTextCommitted(this, &SVehiclePhATToolsPanel::OnParentCommitted)]
                + SVerticalBox::Slot().AutoHeight().Padding(4)
                [
                    SNew(SUniformGridPanel).SlotPadding(3)
                    + SUniformGridPanel::Slot(0, 0)
                    [SNew(SButton).Text(LOCTEXT("CopyBody", "Copy Body Settings")).OnClicked(this, &SVehiclePhATToolsPanel::OnCopyBody)]
                    + SUniformGridPanel::Slot(1, 0)
                    [SNew(SButton).Text(LOCTEXT("PasteBody", "Paste Body Settings")).OnClicked(this, &SVehiclePhATToolsPanel::OnPasteBody)]
                    + SUniformGridPanel::Slot(0, 1)
                    [SNew(SButton).Text(LOCTEXT("CopyTransform", "Copy Transform")).OnClicked(this, &SVehiclePhATToolsPanel::OnCopyTransform)]
                    + SUniformGridPanel::Slot(1, 1)
                    [SNew(SButton).Text(LOCTEXT("PasteTransform", "Paste Transform")).OnClicked(this, &SVehiclePhATToolsPanel::OnPasteTransform)]
                    + SUniformGridPanel::Slot(0, 2)
                    [SNew(SButton).Text(LOCTEXT("Mirror", "Mirror Selected Bodies")).OnClicked(this, &SVehiclePhATToolsPanel::OnMirror)]
                    + SUniformGridPanel::Slot(1, 2)
                    [SNew(SButton).Text(LOCTEXT("Constraint", "Create Constraint")).OnClicked(this, &SVehiclePhATToolsPanel::OnConstraint)]
                    + SUniformGridPanel::Slot(0, 3)
                    [SNew(SButton).Text(LOCTEXT("ConvexCreate", "Create Convex Bodies")).OnClicked(this, &SVehiclePhATToolsPanel::OnConvexCreate)]
                    + SUniformGridPanel::Slot(1, 3)
                    [SNew(SButton).Text(LOCTEXT("ConvexEdit", "Edit Convex Bodies")).OnClicked(this, &SVehiclePhATToolsPanel::OnConvexEdit)]
                    + SUniformGridPanel::Slot(0, 4)
                    [SNew(SButton).Text(LOCTEXT("Validate", "Validate Physics Asset")).OnClicked(this, &SVehiclePhATToolsPanel::OnValidate)]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(4)
                [SNew(STextBlock).AutoWrapText(true).Text(this, &SVehiclePhATToolsPanel::GetStatusText)]
            ]
        ];
    }

private:
    UPhysicsAsset* PhysicsAsset = nullptr;
    FName Bone;
    FName ParentBone;
    FString Status;

    void RefreshSelection()
    {
        PhysicsAsset = FVehiclePhATBodyUtils::GetSelectedPhysicsAsset();
        if (PhysicsAsset && PhysicsAsset->SkeletalBodySetups.Num() > 0 && PhysicsAsset->SkeletalBodySetups[0])
        {
            Bone = PhysicsAsset->SkeletalBodySetups[0]->BoneName;
            ParentBone = Bone;
        }

        Status = PhysicsAsset ? TEXT("Ready.") : TEXT("Select a PhysicsAsset in Content Browser or editor selection.");
    }

    FText GetAssetText() const
    {
        return FText::FromString(PhysicsAsset ? FString::Printf(TEXT("PhysicsAsset: %s"), *PhysicsAsset->GetName()) : TEXT("PhysicsAsset: <none>"));
    }

    FText GetBodiesText() const
    {
        return FText::FromString(PhysicsAsset ? FString::Printf(TEXT("Bodies: %d  Constraints: %d"), PhysicsAsset->SkeletalBodySetups.Num(), PhysicsAsset->ConstraintSetup.Num()) : TEXT("Bodies: 0"));
    }

    FText GetBoneText() const { return FText::FromName(Bone); }
    FText GetParentText() const { return FText::FromName(ParentBone); }
    FText GetStatusText() const { return FText::FromString(Status); }

    void OnBoneCommitted(const FText& Text, ETextCommit::Type) { Bone = VehiclePhATToolsUI::TextToName(Text); }
    void OnParentCommitted(const FText& Text, ETextCommit::Type) { ParentBone = VehiclePhATToolsUI::TextToName(Text); }

    USkeletalBodySetup* GetCurrentBody() const
    {
        return FVehiclePhATBodyUtils::FindBodySetup(PhysicsAsset, Bone);
    }

    FReply OnRefresh()
    {
        RefreshSelection();
        return FReply::Handled();
    }

    FReply OnCopyBody()
    {
        Status = FVehiclePhATClipboard::CopyBodySettings(GetCurrentBody()) ? TEXT("Body settings copied.") : TEXT("Select a valid body.");
        return FReply::Handled();
    }

    FReply OnPasteBody()
    {
        if (!PhysicsAsset)
        {
            Status = TEXT("No PhysicsAsset selected.");
            return FReply::Handled();
        }

        FScopedTransaction Transaction(LOCTEXT("PasteBodyTx", "Paste Vehicle Body Settings"));
        PhysicsAsset->Modify();

        if (USkeletalBodySetup* BodySetup = GetCurrentBody())
        {
            BodySetup->Modify();
            FString Message;
            FVehiclePhATClipboard::PasteBodySettings(BodySetup, Message);
            FVehiclePhATBodyUtils::MarkAssetChanged(PhysicsAsset);
            Status = Message;
        }
        else
        {
            Status = FString::Printf(TEXT("Body '%s' was not found."), *Bone.ToString());
        }

        return FReply::Handled();
    }

    FReply OnCopyTransform()
    {
        Status = FVehiclePhATClipboard::CopyTransform(GetCurrentBody()) ? TEXT("Transform copied.") : TEXT("Select a valid body.");
        return FReply::Handled();
    }

    FReply OnPasteTransform()
    {
        if (!PhysicsAsset)
        {
            Status = TEXT("No PhysicsAsset selected.");
            return FReply::Handled();
        }

        FScopedTransaction Transaction(LOCTEXT("PasteTransformTx", "Paste Vehicle Body Transform"));
        PhysicsAsset->Modify();

        if (USkeletalBodySetup* BodySetup = GetCurrentBody())
        {
            BodySetup->Modify();
            FString Message;
            FVehiclePhATClipboard::PasteTransform(BodySetup, true, true, false, true, Message);
            FVehiclePhATBodyUtils::MarkAssetChanged(PhysicsAsset);
            Status = Message;
        }
        else
        {
            Status = FString::Printf(TEXT("Body '%s' was not found."), *Bone.ToString());
        }

        return FReply::Handled();
    }

    FReply OnMirror()
    {
        if (!PhysicsAsset)
        {
            Status = TEXT("No PhysicsAsset selected.");
            return FReply::Handled();
        }

        VehiclePhATToolsUI::ShowModalWindow(
            LOCTEXT("MirrorWindowTitle", "Mirror Bodies"),
            SNew(SMirrorBodiesDialog).PhysicsAsset(PhysicsAsset),
            FVector2D(720.f, 520.f));
        return FReply::Handled();
    }

    FReply OnConstraint()
    {
        if (!PhysicsAsset)
        {
            Status = TEXT("No PhysicsAsset selected.");
            return FReply::Handled();
        }

        VehiclePhATToolsUI::ShowModalWindow(
            LOCTEXT("ConstraintWindowTitle", "Create Vehicle Constraint"),
            SNew(SCreateConstraintDialog).PhysicsAsset(PhysicsAsset).DefaultParentBone(ParentBone).DefaultChildBone(Bone),
            FVector2D(560.f, 360.f));
        return FReply::Handled();
    }

    FReply OnConvexCreate()
    {
        Status = TEXT("MVP convex tool: backend can add FKConvexElem from point cloud; interactive vertex picker remains next pass.");
        return FReply::Handled();
    }

    FReply OnConvexEdit()
    {
        Status = TEXT("MVP convex edit: backend can replace FKConvexElem from edited point cloud; interactive editor remains next pass.");
        return FReply::Handled();
    }

    FReply OnValidate()
    {
        const TArray<FVehiclePhATValidationMessage> Results = FVehiclePhATBodyUtils::ValidatePhysicsAsset(PhysicsAsset);
        Status = FString::Printf(TEXT("Validation messages: %d"), Results.Num());
        for (const FVehiclePhATValidationMessage& Message : Results)
        {
            Status += TEXT("\n- ") + Message.Message;
        }
        return FReply::Handled();
    }
};

void FVehiclePhATToolsModule::StartupModule()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        VehiclePhATToolsTabName,
        FOnSpawnTab::CreateRaw(this, &FVehiclePhATToolsModule::SpawnVehiclePhATToolsTab))
        .SetDisplayName(LOCTEXT("TabTitle", "Vehicle PhAT Tools"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);

    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FVehiclePhATToolsModule::RegisterMenus));
    UE_LOG(LogVehiclePhATTools, Log, TEXT("VehiclePhATTools started"));
}

void FVehiclePhATToolsModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(VehiclePhATToolsTabName);
}

void FVehiclePhATToolsModule::RegisterMenus()
{
    FToolMenuOwnerScoped Owner(this);
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
    FToolMenuSection& Section = Menu->FindOrAddSection("VehiclePhATTools");
    Section.AddMenuEntry(
        "OpenVehiclePhATTools",
        LOCTEXT("Open", "Vehicle PhAT Tools"),
        LOCTEXT("OpenTooltip", "Open Vehicle PhAT Tools panel"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(this, &FVehiclePhATToolsModule::OpenVehiclePhATToolsTab)));
}

void FVehiclePhATToolsModule::OpenVehiclePhATToolsTab()
{
    FGlobalTabmanager::Get()->TryInvokeTab(VehiclePhATToolsTabName);
}

TSharedRef<SDockTab> FVehiclePhATToolsModule::SpawnVehiclePhATToolsTab(const FSpawnTabArgs&)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SVehiclePhATToolsPanel)
        ];
}

IMPLEMENT_MODULE(FVehiclePhATToolsModule, VehiclePhATTools)

#undef LOCTEXT_NAMESPACE
