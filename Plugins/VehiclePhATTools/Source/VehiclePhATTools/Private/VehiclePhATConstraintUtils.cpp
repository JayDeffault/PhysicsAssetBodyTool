#include "VehiclePhATConstraintUtils.h"

#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "Engine/SkeletalMesh.h"
#include "ScopedTransaction.h"
#include "VehiclePhATBodyUtils.h"
#include "VehiclePhATToolsLog.h"

namespace
{
bool GetRefSkeletonComponentTransform(const FReferenceSkeleton& ReferenceSkeleton, const int32 BoneIndex, FTransform& OutTransform)
{
    if (BoneIndex < 0 || BoneIndex >= ReferenceSkeleton.GetNum())
    {
        return false;
    }

    TArray<FTransform> ComponentSpaceTransforms;
    ComponentSpaceTransforms.SetNum(ReferenceSkeleton.GetNum());
    for (int32 Index = 0; Index < ReferenceSkeleton.GetNum(); ++Index)
    {
        const int32 ParentIndex = ReferenceSkeleton.GetParentIndex(Index);
        const FTransform& LocalTransform = ReferenceSkeleton.GetRefBonePose()[Index];
        ComponentSpaceTransforms[Index] = ParentIndex == INDEX_NONE
            ? LocalTransform
            : LocalTransform * ComponentSpaceTransforms[ParentIndex];
    }

    OutTransform = ComponentSpaceTransforms[BoneIndex];
    return true;
}

bool BuildConstraintFramesFromReferenceSkeleton(
    const UPhysicsAsset* PhysicsAsset,
    const FName ParentBone,
    const FName ChildBone,
    const EVehiclePhATConstraintPreset Preset,
    const float Degrees,
    const bool bFlipAxis,
    FTransform& OutParentFrame,
    FTransform& OutChildFrame)
{
    if (!PhysicsAsset || !PhysicsAsset->PreviewSkeletalMesh.Get())
    {
        return false;
    }

    const FReferenceSkeleton& ReferenceSkeleton = PhysicsAsset->PreviewSkeletalMesh.Get()->GetRefSkeleton();
    const int32 ParentBoneIndex = ReferenceSkeleton.FindBoneIndex(ParentBone);
    const int32 ChildBoneIndex = ReferenceSkeleton.FindBoneIndex(ChildBone);
    if (ParentBoneIndex == INDEX_NONE || ChildBoneIndex == INDEX_NONE)
    {
        return false;
    }

    FTransform ParentComponentTransform;
    FTransform ChildComponentTransform;
    if (!GetRefSkeletonComponentTransform(ReferenceSkeleton, ParentBoneIndex, ParentComponentTransform)
        || !GetRefSkeletonComponentTransform(ReferenceSkeleton, ChildBoneIndex, ChildComponentTransform))
    {
        return false;
    }

    OutParentFrame = ChildComponentTransform.GetRelativeTransform(ParentComponentTransform);
    OutChildFrame = FTransform::Identity;

    const float SignedLimitDegrees = bFlipAxis ? -Degrees : Degrees;
    FRotator ParentFrameBias = FRotator::ZeroRotator;
    FRotator ChildFrameRotation = FRotator::ZeroRotator;

    if (Preset == EVehiclePhATConstraintPreset::Bonnet65 || Preset == EVehiclePhATConstraintPreset::Boot70)
    {
        ChildFrameRotation = FRotator(0.f, 90.f, 0.f);
        ParentFrameBias.Pitch = SignedLimitDegrees;
    }
    else
    {
        ParentFrameBias.Yaw = SignedLimitDegrees;
    }

    if (bFlipAxis)
    {
        ChildFrameRotation.Yaw += 180.f;
    }

    OutParentFrame.ConcatenateRotation(ParentFrameBias.Quaternion());
    OutParentFrame.NormalizeRotation();
    OutChildFrame.ConcatenateRotation(ChildFrameRotation.Quaternion());
    OutChildFrame.NormalizeRotation();
    return true;
}
}

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
    Instance.SetDisableCollision(Options.bDisableCollision);

    FTransform ParentFrame = FTransform::Identity;
    FTransform ChildFrame = FTransform::Identity;
    if (!BuildConstraintFramesFromReferenceSkeleton(
            PhysicsAsset,
            Options.ParentBone,
            Options.ChildBone,
            Options.Preset,
            Degrees,
            Options.bFlipAxis,
            ParentFrame,
            ChildFrame))
    {
        UE_LOG(
            LogVehiclePhATTools,
            Warning,
            TEXT("Could not derive constraint frames from reference skeleton for %s -> %s. Falling back to identity frames."),
            *Options.ParentBone.ToString(),
            *Options.ChildBone.ToString());
    }

    Instance.SetRefFrame(EConstraintFrame::Frame1, ParentFrame);
    Instance.SetRefFrame(EConstraintFrame::Frame2, ChildFrame);

    FVehiclePhATBodyUtils::MarkAssetChanged(PhysicsAsset);
    OutMessage = FString::Printf(
        TEXT("Constraint %s between %s and %s at %.1f degrees. DisableCollision=%s."),
        bHadExistingConstraint ? TEXT("updated") : TEXT("created"),
        *Options.ParentBone.ToString(),
        *Options.ChildBone.ToString(),
        Degrees,
        Options.bDisableCollision ? TEXT("true") : TEXT("false"));

    UE_LOG(LogVehiclePhATTools, Log, TEXT("%s"), *OutMessage);
    return true;
}
