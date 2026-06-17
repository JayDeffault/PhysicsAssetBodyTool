#include "VehiclePhATToolsModule.h"

#include "Framework/Application/SlateApplication.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"
#include "VehiclePhATBodyUtils.h"
#include "VehiclePhATClipboard.h"
#include "VehiclePhATConstraintUtils.h"
#include "VehiclePhATConvexUtils.h"
#include "VehiclePhATMirrorUtils.h"
#include "VehiclePhATNativeConvexTool.h"
#include "VehiclePhATToolsLog.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
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

static FText ConstraintPresetToText(EVehiclePhATConstraintPreset Preset)
{
    switch (Preset)
    {
    case EVehiclePhATConstraintPreset::Door60:
        return LOCTEXT("PresetDoor60", "Door 60 degrees");
    case EVehiclePhATConstraintPreset::Door70:
        return LOCTEXT("PresetDoor70", "Door 70 degrees");
    case EVehiclePhATConstraintPreset::Bonnet65:
        return LOCTEXT("PresetBonnet65", "Bonnet 65 degrees");
    case EVehiclePhATConstraintPreset::Boot70:
        return LOCTEXT("PresetBoot70", "Boot 70 degrees");
    case EVehiclePhATConstraintPreset::Custom:
    default:
        return LOCTEXT("PresetCustom", "Custom");
    }
}

static EVehiclePhATConstraintPreset PresetFromLabel(const FString& Label)
{
    if (Label.Contains(TEXT("Door 60")))
    {
        return EVehiclePhATConstraintPreset::Door60;
    }
    if (Label.Contains(TEXT("Door 70")))
    {
        return EVehiclePhATConstraintPreset::Door70;
    }
    if (Label.Contains(TEXT("Bonnet")))
    {
        return EVehiclePhATConstraintPreset::Bonnet65;
    }
    if (Label.Contains(TEXT("Boot")))
    {
        return EVehiclePhATConstraintPreset::Boot70;
    }
    return EVehiclePhATConstraintPreset::Custom;
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
            FString ShapeSummary = TEXT("source body missing");
            if (const USkeletalBodySetup* SourceBody = FVehiclePhATBodyUtils::FindBodySetup(PhysicsAsset, Pair.SourceBone))
            {
                const FKAggregateGeom& AggGeom = SourceBody->AggGeom;
                ShapeSummary = FString::Printf(
                    TEXT("boxes=%d spheres=%d capsules=%d tapered=%d convex=%d"),
                    AggGeom.BoxElems.Num(),
                    AggGeom.SphereElems.Num(),
                    AggGeom.SphylElems.Num(),
                    AggGeom.TaperedCapsuleElems.Num(),
                    AggGeom.ConvexElems.Num());
            }

            PreviewText += FString::Printf(
                TEXT("\n[%s] %s -> %s | source body: %s | target bone: %s | target body: %s | %s"),
                Pair.bSelected ? TEXT("x") : TEXT(" "),
                *Pair.SourceBone.ToString(),
                *Pair.TargetBone.ToString(),
                Pair.bSourceBodyExists ? TEXT("yes") : TEXT("no"),
                Pair.bTargetBoneExists ? TEXT("yes") : TEXT("no"),
                Pair.bTargetBodyExists ? TEXT("yes") : TEXT("no"),
                *ShapeSummary);
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
        Options.bUpdateExisting = true;

        PresetLabels.Add(MakeShared<FString>(VehiclePhATToolsUI::ConstraintPresetToText(EVehiclePhATConstraintPreset::Door60).ToString()));
        PresetLabels.Add(MakeShared<FString>(VehiclePhATToolsUI::ConstraintPresetToText(EVehiclePhATConstraintPreset::Door70).ToString()));
        PresetLabels.Add(MakeShared<FString>(VehiclePhATToolsUI::ConstraintPresetToText(EVehiclePhATConstraintPreset::Bonnet65).ToString()));
        PresetLabels.Add(MakeShared<FString>(VehiclePhATToolsUI::ConstraintPresetToText(EVehiclePhATConstraintPreset::Boot70).ToString()));
        PresetLabels.Add(MakeShared<FString>(VehiclePhATToolsUI::ConstraintPresetToText(EVehiclePhATConstraintPreset::Custom).ToString()));
        SelectedPresetLabel = PresetLabels[1];

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
                [SNew(STextBlock).Text(LOCTEXT("ConstraintPreset", "Preset"))]
                + SUniformGridPanel::Slot(1, 2)
                [
                    SNew(SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&PresetLabels)
                    .InitiallySelectedItem(SelectedPresetLabel)
                    .OnGenerateWidget(this, &SCreateConstraintDialog::GeneratePresetWidget)
                    .OnSelectionChanged(this, &SCreateConstraintDialog::OnPresetChanged)
                    [
                        SNew(STextBlock).Text(this, &SCreateConstraintDialog::GetPresetText)
                    ]
                ]
                + SUniformGridPanel::Slot(0, 3)
                [SNew(STextBlock).Text(LOCTEXT("AngularLimit", "Angular limit degrees"))]
                + SUniformGridPanel::Slot(1, 3)
                [SNew(SNumericEntryBox<float>).Value(this, &SCreateConstraintDialog::GetAngularLimit).MinValue(0.f).MaxValue(180.f).OnValueChanged(this, &SCreateConstraintDialog::OnAngularLimitChanged)]
                + SUniformGridPanel::Slot(0, 4)
                [SNew(STextBlock).Text(LOCTEXT("UpdateExisting", "Update existing"))]
                + SUniformGridPanel::Slot(1, 4)
                [SNew(SCheckBox).IsChecked(this, &SCreateConstraintDialog::GetUpdateExistingState).OnCheckStateChanged(this, &SCreateConstraintDialog::OnUpdateExistingChanged)]
                + SUniformGridPanel::Slot(0, 5)
                [SNew(STextBlock).Text(LOCTEXT("FlipAxis", "Flip axis"))]
                + SUniformGridPanel::Slot(1, 5)
                [SNew(SCheckBox).IsChecked(this, &SCreateConstraintDialog::GetFlipAxisState).OnCheckStateChanged(this, &SCreateConstraintDialog::OnFlipAxisChanged)]
                + SUniformGridPanel::Slot(0, 6)
                [SNew(STextBlock).Text(LOCTEXT("DisableCollision", "Disable collision"))]
                + SUniformGridPanel::Slot(1, 6)
                [SNew(SCheckBox).IsChecked(this, &SCreateConstraintDialog::GetDisableCollisionState).OnCheckStateChanged(this, &SCreateConstraintDialog::OnDisableCollisionChanged)]
            ]
            + SVerticalBox::Slot().FillHeight(1.f).Padding(6)
            [SNew(STextBlock).Text(this, &SCreateConstraintDialog::GetStatusText).AutoWrapText(true)]
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel).SlotPadding(4)
                + SUniformGridPanel::Slot(0, 0)
                [SNew(SButton).Text(LOCTEXT("FlipAxisButton", "Flip Axis")).OnClicked(this, &SCreateConstraintDialog::OnFlipAxisButton)]
                + SUniformGridPanel::Slot(1, 0)
                [SNew(SButton).Text(LOCTEXT("ApplyConstraint", "Apply Constraint")).OnClicked(this, &SCreateConstraintDialog::OnApply)]
            ]
        ];
    }

private:
    UPhysicsAsset* PhysicsAsset = nullptr;
    FVehiclePhATConstraintOptions Options;
    FString Status;
    TArray<TSharedPtr<FString>> PresetLabels;
    TSharedPtr<FString> SelectedPresetLabel;

    FText GetParentBoneText() const { return FText::FromName(Options.ParentBone); }
    FText GetChildBoneText() const { return FText::FromName(Options.ChildBone); }
    FText GetPresetText() const { return VehiclePhATToolsUI::ConstraintPresetToText(Options.Preset); }
    TOptional<float> GetAngularLimit() const { return Options.AngularLimitDegrees; }
    FText GetStatusText() const { return FText::FromString(Status); }
    ECheckBoxState GetUpdateExistingState() const { return Options.bUpdateExisting ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
    ECheckBoxState GetFlipAxisState() const { return Options.bFlipAxis ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
    ECheckBoxState GetDisableCollisionState() const { return Options.bDisableCollision ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }

    void OnParentBoneCommitted(const FText& Text, ETextCommit::Type) { Options.ParentBone = VehiclePhATToolsUI::TextToName(Text); }
    void OnChildBoneCommitted(const FText& Text, ETextCommit::Type) { Options.ChildBone = VehiclePhATToolsUI::TextToName(Text); }
    void OnAngularLimitChanged(float NewValue) { Options.AngularLimitDegrees = NewValue; Options.Preset = EVehiclePhATConstraintPreset::Custom; }
    void OnUpdateExistingChanged(ECheckBoxState State) { Options.bUpdateExisting = State == ECheckBoxState::Checked; }
    void OnFlipAxisChanged(ECheckBoxState State) { Options.bFlipAxis = State == ECheckBoxState::Checked; }
    void OnDisableCollisionChanged(ECheckBoxState State) { Options.bDisableCollision = State == ECheckBoxState::Checked; }

    TSharedRef<SWidget> GeneratePresetWidget(TSharedPtr<FString> Item) const
    {
        return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
    }

    void OnPresetChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type)
    {
        if (!NewSelection.IsValid())
        {
            return;
        }

        SelectedPresetLabel = NewSelection;
        Options.Preset = VehiclePhATToolsUI::PresetFromLabel(*NewSelection);
        Options.AngularLimitDegrees = FVehiclePhATConstraintUtils::PresetDegrees(Options.Preset, Options.AngularLimitDegrees);
        Status = FString::Printf(TEXT("Preset selected: %s"), *(*NewSelection));
    }

    FReply OnFlipAxisButton()
    {
        Options.bFlipAxis = !Options.bFlipAxis;
        Status = Options.bFlipAxis ? TEXT("Axis flipped. Apply to update/create the constraint.") : TEXT("Axis restored. Apply to update/create the constraint.");
        return FReply::Handled();
    }

    FReply OnApply()
    {
        FString Message;
        FVehiclePhATConstraintUtils::CreateOrUpdateConstraint(PhysicsAsset, Options, Message);
        Status = Message;
        return FReply::Handled();
    }
};

class SConvexCreationDialog final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SConvexCreationDialog) {}
        SLATE_ARGUMENT(UPhysicsAsset*, PhysicsAsset)
        SLATE_ARGUMENT(FName, DefaultBone)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        PhysicsAsset = InArgs._PhysicsAsset;
        BoneName = InArgs._DefaultBone;
        SeedPointsFromCurrentBody();

        ChildSlot
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ConvexCreateHelp", "MVP 4 convex creation: select a body bone, edit/paste local-space point coordinates, preview count, then apply as FKConvexElem. One point per line: X Y Z."))
                .AutoWrapText(true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel).SlotPadding(4)
                + SUniformGridPanel::Slot(0, 0)
                [SNew(STextBlock).Text(LOCTEXT("ConvexBone", "Target body bone"))]
                + SUniformGridPanel::Slot(1, 0)
                [SNew(SEditableTextBox).Text(this, &SConvexCreationDialog::GetBoneText).OnTextCommitted(this, &SConvexCreationDialog::OnBoneCommitted)]
            ]
            + SVerticalBox::Slot().FillHeight(1.f).Padding(6)
            [
                SAssignNew(PointsTextBox, SMultiLineEditableTextBox)
                .Text(this, &SConvexCreationDialog::GetPointsText)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [SNew(STextBlock).Text(this, &SConvexCreationDialog::GetStatusText).AutoWrapText(true)]
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel).SlotPadding(4)
                + SUniformGridPanel::Slot(0, 0)
                [SNew(SButton).Text(LOCTEXT("SeedConvex", "Seed From Body Shape")).OnClicked(this, &SConvexCreationDialog::OnSeedFromBody)]
                + SUniformGridPanel::Slot(1, 0)
                [SNew(SButton).Text(LOCTEXT("PreviewConvex", "Preview Points")).OnClicked(this, &SConvexCreationDialog::OnPreview)]
                + SUniformGridPanel::Slot(2, 0)
                [SNew(SButton).Text(LOCTEXT("ApplyConvex", "Apply Convex")).OnClicked(this, &SConvexCreationDialog::OnApply)]
            ]
        ];
    }

private:
    UPhysicsAsset* PhysicsAsset = nullptr;
    FName BoneName;
    FString PointsText;
    FString Status;
    TSharedPtr<SMultiLineEditableTextBox> PointsTextBox;

    FText GetBoneText() const { return FText::FromName(BoneName); }
    FText GetPointsText() const { return FText::FromString(PointsText); }
    FText GetStatusText() const { return FText::FromString(Status); }

    void OnBoneCommitted(const FText& Text, ETextCommit::Type)
    {
        BoneName = VehiclePhATToolsUI::TextToName(Text);
        SeedPointsFromCurrentBody();
        if (PointsTextBox.IsValid())
        {
            PointsTextBox->SetText(FText::FromString(PointsText));
        }
    }

    FReply OnSeedFromBody()
    {
        SeedPointsFromCurrentBody();
        if (PointsTextBox.IsValid())
        {
            PointsTextBox->SetText(FText::FromString(PointsText));
        }
        return FReply::Handled();
    }

    FReply OnPreview()
    {
        const TArray<FVector> Points = ParsePointsFromText();
        Status = FString::Printf(TEXT("Preview: %d point(s). At least 4 non-coplanar points are required."), Points.Num());
        return FReply::Handled();
    }

    FReply OnApply()
    {
        const TArray<FVector> Points = ParsePointsFromText();
        if (USkeletalBodySetup* BodySetup = FVehiclePhATBodyUtils::FindBodySetup(PhysicsAsset, BoneName))
        {
            FString Message;
            FVehiclePhATConvexUtils::AddConvexFromPoints(PhysicsAsset, BodySetup, Points, Message);
            Status = Message;
        }
        else
        {
            Status = FString::Printf(TEXT("Body '%s' was not found."), *BoneName.ToString());
        }
        return FReply::Handled();
    }

    TArray<FVector> ParsePointsFromText() const
    {
        const FString Source = PointsTextBox.IsValid() ? PointsTextBox->GetText().ToString() : PointsText;
        TArray<FString> Lines;
        Source.ParseIntoArrayLines(Lines, true);

        TArray<FVector> Points;
        for (FString Line : Lines)
        {
            Line.ReplaceInline(TEXT(","), TEXT(" "));
            TArray<FString> Tokens;
            Line.ParseIntoArrayWS(Tokens);
            if (Tokens.Num() < 3)
            {
                continue;
            }

            Points.Add(FVector(FCString::Atof(*Tokens[0]), FCString::Atof(*Tokens[1]), FCString::Atof(*Tokens[2])));
        }
        return Points;
    }

    void SeedPointsFromCurrentBody()
    {
        const USkeletalBodySetup* BodySetup = FVehiclePhATBodyUtils::FindBodySetup(PhysicsAsset, BoneName);
        TArray<FVector> Points;

        if (BodySetup && BodySetup->AggGeom.BoxElems.Num() > 0)
        {
            const FKBoxElem& Box = BodySetup->AggGeom.BoxElems[0];
            const FVector HalfExtents(Box.X * 0.5f, Box.Y * 0.5f, Box.Z * 0.5f);
            const FTransform BoxTransform = Box.GetTransform();
            for (const float XSign : {-1.f, 1.f})
            {
                for (const float YSign : {-1.f, 1.f})
                {
                    for (const float ZSign : {-1.f, 1.f})
                    {
                        Points.Add(BoxTransform.TransformPosition(FVector(XSign * HalfExtents.X, YSign * HalfExtents.Y, ZSign * HalfExtents.Z)));
                    }
                }
            }
        }
        else if (BodySetup && BodySetup->AggGeom.SphereElems.Num() > 0)
        {
            const FKSphereElem& Sphere = BodySetup->AggGeom.SphereElems[0];
            AppendCubePoints(Points, Sphere.Center, FVector(Sphere.Radius));
        }

        if (Points.Num() == 0)
        {
            AppendCubePoints(Points, FVector::ZeroVector, FVector(10.f));
        }

        PointsText.Reset();
        for (const FVector& Point : Points)
        {
            PointsText += FString::Printf(TEXT("%.3f %.3f %.3f\n"), Point.X, Point.Y, Point.Z);
        }
        Status = FString::Printf(TEXT("Seeded %d point(s) for body '%s'."), Points.Num(), *BoneName.ToString());
    }

    static void AppendCubePoints(TArray<FVector>& OutPoints, const FVector& Center, const FVector& HalfExtents)
    {
        for (const float XSign : {-1.f, 1.f})
        {
            for (const float YSign : {-1.f, 1.f})
            {
                for (const float ZSign : {-1.f, 1.f})
                {
                    OutPoints.Add(Center + FVector(XSign * HalfExtents.X, YSign * HalfExtents.Y, ZSign * HalfExtents.Z));
                }
            }
        }
    }

};

class SConvexEditDialog final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SConvexEditDialog) {}
        SLATE_ARGUMENT(UPhysicsAsset*, PhysicsAsset)
        SLATE_ARGUMENT(FName, DefaultBone)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        PhysicsAsset = InArgs._PhysicsAsset;
        BoneName = InArgs._DefaultBone;
        LoadCurrentConvex();

        ChildSlot
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ConvexEditHelp", "MVP 5 convex edit: extract existing FKConvexElem vertices, edit the point cloud, then rebuild/replace that convex element. One local-space point per line: X Y Z."))
                .AutoWrapText(true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel).SlotPadding(4)
                + SUniformGridPanel::Slot(0, 0)
                [SNew(STextBlock).Text(LOCTEXT("ConvexEditBone", "Body bone"))]
                + SUniformGridPanel::Slot(1, 0)
                [SNew(SEditableTextBox).Text(this, &SConvexEditDialog::GetBoneText).OnTextCommitted(this, &SConvexEditDialog::OnBoneCommitted)]
                + SUniformGridPanel::Slot(0, 1)
                [SNew(STextBlock).Text(LOCTEXT("ConvexEditIndex", "Convex index"))]
                + SUniformGridPanel::Slot(1, 1)
                [SNew(SNumericEntryBox<int32>).Value(this, &SConvexEditDialog::GetConvexIndex).MinValue(0).OnValueChanged(this, &SConvexEditDialog::OnConvexIndexChanged)]
            ]
            + SVerticalBox::Slot().FillHeight(1.f).Padding(6)
            [
                SAssignNew(PointsTextBox, SMultiLineEditableTextBox)
                .Text(this, &SConvexEditDialog::GetPointsText)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [SNew(STextBlock).Text(this, &SConvexEditDialog::GetStatusText).AutoWrapText(true)]
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel).SlotPadding(4)
                + SUniformGridPanel::Slot(0, 0)
                [SNew(SButton).Text(LOCTEXT("LoadConvex", "Load Convex")).OnClicked(this, &SConvexEditDialog::OnLoad)]
                + SUniformGridPanel::Slot(1, 0)
                [SNew(SButton).Text(LOCTEXT("PreviewEditedConvex", "Preview Points")).OnClicked(this, &SConvexEditDialog::OnPreview)]
                + SUniformGridPanel::Slot(2, 0)
                [SNew(SButton).Text(LOCTEXT("ReplaceConvex", "Rebuild / Replace")).OnClicked(this, &SConvexEditDialog::OnReplace)]
            ]
        ];
    }

private:
    UPhysicsAsset* PhysicsAsset = nullptr;
    FName BoneName;
    int32 ConvexIndex = 0;
    FString PointsText;
    FString Status;
    TSharedPtr<SMultiLineEditableTextBox> PointsTextBox;

    FText GetBoneText() const { return FText::FromName(BoneName); }
    TOptional<int32> GetConvexIndex() const { return ConvexIndex; }
    FText GetPointsText() const { return FText::FromString(PointsText); }
    FText GetStatusText() const { return FText::FromString(Status); }

    void OnBoneCommitted(const FText& Text, ETextCommit::Type)
    {
        BoneName = VehiclePhATToolsUI::TextToName(Text);
        ConvexIndex = 0;
        LoadCurrentConvex();
        SyncTextBox();
    }

    void OnConvexIndexChanged(int32 NewValue)
    {
        ConvexIndex = FMath::Max(0, NewValue);
    }

    FReply OnLoad()
    {
        LoadCurrentConvex();
        SyncTextBox();
        return FReply::Handled();
    }

    FReply OnPreview()
    {
        const TArray<FVector> Points = ParsePointsFromText();
        Status = FString::Printf(TEXT("Preview edited convex %d on '%s': %d point(s)."), ConvexIndex, *BoneName.ToString(), Points.Num());
        return FReply::Handled();
    }

    FReply OnReplace()
    {
        USkeletalBodySetup* BodySetup = FVehiclePhATBodyUtils::FindBodySetup(PhysicsAsset, BoneName);
        if (!BodySetup)
        {
            Status = FString::Printf(TEXT("Body '%s' was not found."), *BoneName.ToString());
            return FReply::Handled();
        }

        const TArray<FVector> Points = ParsePointsFromText();
        FString Message;
        FVehiclePhATConvexUtils::ReplaceConvexFromPoints(PhysicsAsset, BodySetup, ConvexIndex, Points, Message);
        Status = Message;
        return FReply::Handled();
    }

    void LoadCurrentConvex()
    {
        PointsText.Reset();
        const USkeletalBodySetup* BodySetup = FVehiclePhATBodyUtils::FindBodySetup(PhysicsAsset, BoneName);
        if (!BodySetup)
        {
            Status = FString::Printf(TEXT("Body '%s' was not found."), *BoneName.ToString());
            return;
        }

        if (!BodySetup->AggGeom.ConvexElems.IsValidIndex(ConvexIndex))
        {
            Status = FString::Printf(TEXT("Body '%s' has %d convex element(s); index %d is invalid."), *BoneName.ToString(), BodySetup->AggGeom.ConvexElems.Num(), ConvexIndex);
            return;
        }

        const FKConvexElem& Convex = BodySetup->AggGeom.ConvexElems[ConvexIndex];
        for (const FVector& Vertex : Convex.VertexData)
        {
            PointsText += FString::Printf(TEXT("%.3f %.3f %.3f\n"), Vertex.X, Vertex.Y, Vertex.Z);
        }
        Status = FString::Printf(TEXT("Loaded convex %d from '%s' with %d vertices."), ConvexIndex, *BoneName.ToString(), Convex.VertexData.Num());
    }

    void SyncTextBox()
    {
        if (PointsTextBox.IsValid())
        {
            PointsTextBox->SetText(FText::FromString(PointsText));
        }
    }

    TArray<FVector> ParsePointsFromText() const
    {
        const FString Source = PointsTextBox.IsValid() ? PointsTextBox->GetText().ToString() : PointsText;
        TArray<FString> Lines;
        Source.ParseIntoArrayLines(Lines, true);

        TArray<FVector> Points;
        for (FString Line : Lines)
        {
            Line.ReplaceInline(TEXT(","), TEXT(" "));
            TArray<FString> Tokens;
            Line.ParseIntoArrayWS(Tokens);
            if (Tokens.Num() < 3)
            {
                continue;
            }
            Points.Add(FVector(FCString::Atof(*Tokens[0]), FCString::Atof(*Tokens[1]), FCString::Atof(*Tokens[2])));
        }
        return Points;
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
                + SVerticalBox::Slot().AutoHeight().Padding(4)
                [
                    SAssignNew(InlineToolHost, SVerticalBox)
                ]
            ]
        ];
    }

private:
    UPhysicsAsset* PhysicsAsset = nullptr;
    FName Bone;
    FName ParentBone;
    FString Status;
    TSharedPtr<SVerticalBox> InlineToolHost;

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
        if (!PhysicsAsset)
        {
            Status = TEXT("No PhysicsAsset selected.");
            return FReply::Handled();
        }

        if (InlineToolHost.IsValid())
        {
            FVehiclePhATNativeConvexTool::StartCreate(PhysicsAsset, Bone);
            InlineToolHost->ClearChildren();
            InlineToolHost->AddSlot().AutoHeight()
            [
                SNew(SConvexCreationDialog).PhysicsAsset(PhysicsAsset).DefaultBone(Bone)
            ];
            Status = TEXT("Create Convex Bodies is active inline. No modal window was opened.");
        }
        return FReply::Handled();
    }

    FReply OnConvexEdit()
    {
        if (!PhysicsAsset)
        {
            Status = TEXT("No PhysicsAsset selected.");
            return FReply::Handled();
        }

        if (InlineToolHost.IsValid())
        {
            FVehiclePhATNativeConvexTool::StartEdit(PhysicsAsset, Bone, 0);
            InlineToolHost->ClearChildren();
            InlineToolHost->AddSlot().AutoHeight()
            [
                SNew(SConvexEditDialog).PhysicsAsset(PhysicsAsset).DefaultBone(Bone)
            ];
            Status = TEXT("Edit Convex Bodies is active inline. No modal window was opened.");
        }
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
