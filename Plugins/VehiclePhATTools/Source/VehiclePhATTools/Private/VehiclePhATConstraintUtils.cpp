#include "VehiclePhATConstraintUtils.h"

#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "ScopedTransaction.h"
#include "VehiclePhATBodyUtils.h"
#include "VehiclePhATToolsLog.h"

UPhysicsConstraintTemplate* FVehiclePhATConstraintUtils::FindConstraint(UPhysicsAsset* PhysicsAsset, FName BoneA, FName BoneB, int32* OutIndex)
{
    if (OutIndex)
    {
        *OutIndex = INDEX_NONE;
    }

    if (!PhysicsAsset)
    {
        return nullptr;
    }

    for (int32 Index = 0; Index < PhysicsAsset->ConstraintSetup.Num(); ++Index)
    {
        UPhysicsConstraintTemplate* ConstraintTemplate = PhysicsAsset->ConstraintSetup[Index];
        if (!ConstraintTemplate)
        {
            continue;
        }

        const FName ConstraintBone1 = ConstraintTemplate->DefaultInstance.ConstraintBone1;
        const FName ConstraintBone2 = ConstraintTemplate->DefaultInstance.ConstraintBone2;
        if ((ConstraintBone1 == BoneA && ConstraintBone2 == BoneB) || (ConstraintBone1 == BoneB && ConstraintBone2 == BoneA))
        {
            if (OutIndex)
            {
                *OutIndex = Index;
            }
            return ConstraintTemplate;
        }
    }

    return nullptr;
}

float FVehiclePhATConstraintUtils::PresetDegrees(EVehiclePhATConstraintPreset Preset, float CustomDegrees)
{
    switch (Preset)
    {
    case EVehiclePhATConstraintPreset::Door60:
        return 60.f;
    case EVehiclePhATConstraintPreset::Door70:
        return 70.f;
    case EVehiclePhATConstraintPreset::Bonnet65:
        return 65.f;
    case EVehiclePhATConstraintPreset::Boot70:
        return 70.f;
    case EVehiclePhATConstraintPreset::Custom:
    default:
        return CustomDegrees;
    }
}

bool FVehiclePhATConstraintUtils::CreateOrUpdateConstraint(UPhysicsAsset* PhysicsAsset, const FVehiclePhATConstraintOptions& Options, FString& OutMessage)
{
    if (!PhysicsAsset)
    {
        OutMessage = TEXT("No PhysicsAsset selected.");
        return false;
    }

    if (!FVehiclePhATBodyUtils::FindBodySetup(PhysicsAsset, Options.ParentBone) || !FVehiclePhATBodyUtils::FindBodySetup(PhysicsAsset, Options.ChildBone))
    {
        OutMessage = TEXT("Parent or child body was not found.");
        return false;
    }

    FScopedTransaction Transaction(NSLOCTEXT("VehiclePhATTools", "CreateConstraint", "Create Vehicle Constraint"));
    PhysicsAsset->Modify();

    const bool bHadExistingConstraint = FindConstraint(PhysicsAsset, Options.ParentBone, Options.ChildBone) != nullptr;
    UPhysicsConstraintTemplate* ConstraintTemplate = FindConstraint(PhysicsAsset, Options.ParentBone, Options.ChildBone);
    if (ConstraintTemplate && !Options.bUpdateExisting)
    {
        OutMessage = TEXT("Constraint already exists; enable Update Existing before applying.");
        return false;
    }

    if (!ConstraintTemplate)
    {
        ConstraintTemplate = NewObject<UPhysicsConstraintTemplate>(PhysicsAsset, NAME_None, RF_Transactional);
        PhysicsAsset->ConstraintSetup.Add(ConstraintTemplate);
    }

    ConstraintTemplate->Modify();
    FConstraintInstance& Instance = ConstraintTemplate->DefaultInstance;
    Instance.ConstraintBone1 = Options.ParentBone;
    Instance.ConstraintBone2 = Options.ChildBone;

    const float Degrees = PresetDegrees(Options.Preset, Options.AngularLimitDegrees);
    Instance.SetAngularSwing1Motion(ACM_Limited);
    Instance.SetAngularSwing2Motion(ACM_Locked);
    Instance.SetAngularTwistMotion(ACM_Locked);
    Instance.SetAngularSwing1Limit(ACM_Limited, Degrees);
    Instance.SetAngularSwing2Limit(ACM_Locked, 0.f);
    Instance.SetAngularTwistLimit(ACM_Locked, 0.f);

    FRotator FrameRotation = FRotator::ZeroRotator;
    if (Options.Preset == EVehiclePhATConstraintPreset::Bonnet65 || Options.Preset == EVehiclePhATConstraintPreset::Boot70)
    {
        FrameRotation = FRotator(0.f, 90.f, 0.f);
    }
    if (Options.bFlipAxis)
    {
        FrameRotation.Yaw += 180.f;
    }

    Instance.SetRefFrame(EConstraintFrame::Frame1, FTransform(FrameRotation));
    Instance.SetRefFrame(EConstraintFrame::Frame2, FTransform(FrameRotation));

    FVehiclePhATBodyUtils::MarkAssetChanged(PhysicsAsset);
    OutMessage = FString::Printf(
        TEXT("Constraint %s between %s and %s at %.1f degrees."),
        bHadExistingConstraint ? TEXT("updated") : TEXT("created"),
        *Options.ParentBone.ToString(),
        *Options.ChildBone.ToString(),
        Degrees);

    UE_LOG(LogVehiclePhATTools, Log, TEXT("%s"), *OutMessage);
    return true;
}
