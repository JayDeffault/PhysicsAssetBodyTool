#include "VehiclePhATBodyUtils.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "VehiclePhATToolsLog.h"

UPhysicsAsset* FVehiclePhATBodyUtils::GetSelectedPhysicsAsset()
{
    const TArray<UPhysicsAsset*> Assets = GetSelectedPhysicsAssets();
    return Assets.Num() > 0 ? Assets[0] : nullptr;
}

TArray<UPhysicsAsset*> FVehiclePhATBodyUtils::GetSelectedPhysicsAssets()
{
    TArray<UPhysicsAsset*> Result;
    if (!GEditor)
    {
        return Result;
    }

    USelection* Selection = GEditor->GetSelectedObjects();
    for (FSelectionIterator It(*Selection); It; ++It)
    {
        if (UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(*It))
        {
            Result.Add(PhysicsAsset);
        }
    }

    return Result;
}

USkeletalBodySetup* FVehiclePhATBodyUtils::FindBodySetup(UPhysicsAsset* PhysicsAsset, FName BoneName, int32* OutIndex)
{
    if (OutIndex)
    {
        *OutIndex = INDEX_NONE;
    }

    if (!PhysicsAsset)
    {
        return nullptr;
    }

    for (int32 Index = 0; Index < PhysicsAsset->SkeletalBodySetups.Num(); ++Index)
    {
        USkeletalBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[Index];
        if (BodySetup && BodySetup->BoneName == BoneName)
        {
            if (OutIndex)
            {
                *OutIndex = Index;
            }
            return BodySetup;
        }
    }

    return nullptr;
}

int32 FVehiclePhATBodyUtils::FindBodySetupIndex(UPhysicsAsset* PhysicsAsset, FName BoneName)
{
    int32 Index = INDEX_NONE;
    FindBodySetup(PhysicsAsset, BoneName, &Index);
    return Index;
}

int32 FVehiclePhATBodyUtils::GetShapeCount(const FKAggregateGeom& AggGeom)
{
    return AggGeom.SphereElems.Num()
        + AggGeom.BoxElems.Num()
        + AggGeom.SphylElems.Num()
        + AggGeom.TaperedCapsuleElems.Num()
        + AggGeom.ConvexElems.Num();
}

bool FVehiclePhATBodyUtils::HasAnyShape(const USkeletalBodySetup* BodySetup)
{
    return BodySetup && GetShapeCount(BodySetup->AggGeom) > 0;
}

TArray<FName> FVehiclePhATBodyUtils::GetSkeletonBoneNames(const UPhysicsAsset* PhysicsAsset)
{
    TArray<FName> Result;
    if (!PhysicsAsset || !PhysicsAsset->PreviewSkeletalMesh.Get())
    {
        return Result;
    }

    const FReferenceSkeleton& ReferenceSkeleton = PhysicsAsset->PreviewSkeletalMesh.Get()->GetRefSkeleton();
    for (int32 Index = 0; Index < ReferenceSkeleton.GetNum(); ++Index)
    {
        Result.Add(ReferenceSkeleton.GetBoneName(Index));
    }

    return Result;
}

void FVehiclePhATBodyUtils::RefreshPhysicsAsset(UPhysicsAsset* PhysicsAsset)
{
    if (!PhysicsAsset)
    {
        return;
    }

    PhysicsAsset->UpdateBodySetupIndexMap();
    PhysicsAsset->InvalidateAllPhysicsMeshes();
}

void FVehiclePhATBodyUtils::MarkAssetChanged(UPhysicsAsset* PhysicsAsset)
{
    if (!PhysicsAsset)
    {
        return;
    }

    RefreshPhysicsAsset(PhysicsAsset);
    PhysicsAsset->PostEditChange();
    PhysicsAsset->MarkPackageDirty();
}

void FVehiclePhATBodyUtils::MarkBodySetupGeometryChanged(UPhysicsAsset* PhysicsAsset, USkeletalBodySetup* BodySetup)
{
    if (!PhysicsAsset || !BodySetup)
    {
        MarkAssetChanged(PhysicsAsset);
        return;
    }

    BodySetup->InvalidatePhysicsData();
    BodySetup->CreatePhysicsMeshes();
    BodySetup->PostEditChange();
    MarkAssetChanged(PhysicsAsset);
}

TArray<FVehiclePhATValidationMessage> FVehiclePhATBodyUtils::ValidatePhysicsAsset(UPhysicsAsset* PhysicsAsset, const FString& MirrorSourcePattern, const FString& MirrorTargetPattern)
{
    TArray<FVehiclePhATValidationMessage> Messages;
    if (!PhysicsAsset)
    {
        Messages.Add({FVehiclePhATValidationMessage::ESeverity::Error, TEXT("No PhysicsAsset selected.")});
        return Messages;
    }

    TSet<FName> BodyBones;
    for (USkeletalBodySetup* BodySetup : PhysicsAsset->SkeletalBodySetups)
    {
        if (!BodySetup)
        {
            Messages.Add({FVehiclePhATValidationMessage::ESeverity::Error, TEXT("Null body setup entry.")});
            continue;
        }

        BodyBones.Add(BodySetup->BoneName);
        if (!HasAnyShape(BodySetup))
        {
            Messages.Add({FVehiclePhATValidationMessage::ESeverity::Warning, FString::Printf(TEXT("Body '%s' has no shapes."), *BodySetup->BoneName.ToString())});
        }

        for (const FKBoxElem& Box : BodySetup->AggGeom.BoxElems)
        {
            if (Box.X <= KINDA_SMALL_NUMBER || Box.Y <= KINDA_SMALL_NUMBER || Box.Z <= KINDA_SMALL_NUMBER)
            {
                Messages.Add({FVehiclePhATValidationMessage::ESeverity::Warning, FString::Printf(TEXT("Body '%s' contains a zero/small box shape."), *BodySetup->BoneName.ToString())});
            }
        }

        for (const FKSphereElem& Sphere : BodySetup->AggGeom.SphereElems)
        {
            if (Sphere.Radius <= KINDA_SMALL_NUMBER)
            {
                Messages.Add({FVehiclePhATValidationMessage::ESeverity::Warning, FString::Printf(TEXT("Body '%s' contains a zero/small sphere shape."), *BodySetup->BoneName.ToString())});
            }
        }

        for (const FKSphylElem& Capsule : BodySetup->AggGeom.SphylElems)
        {
            if (Capsule.Radius <= KINDA_SMALL_NUMBER || Capsule.Length <= KINDA_SMALL_NUMBER)
            {
                Messages.Add({FVehiclePhATValidationMessage::ESeverity::Warning, FString::Printf(TEXT("Body '%s' contains a zero/small capsule shape."), *BodySetup->BoneName.ToString())});
            }
        }
    }

    for (FName BoneName : GetSkeletonBoneNames(PhysicsAsset))
    {
        if (!BodyBones.Contains(BoneName))
        {
            Messages.Add({FVehiclePhATValidationMessage::ESeverity::Info, FString::Printf(TEXT("Bone '%s' has no body."), *BoneName.ToString())});
        }
    }

    TSet<FString> ConstraintKeys;
    for (UPhysicsConstraintTemplate* ConstraintTemplate : PhysicsAsset->ConstraintSetup)
    {
        if (!ConstraintTemplate)
        {
            Messages.Add({FVehiclePhATValidationMessage::ESeverity::Warning, TEXT("Null constraint entry.")});
            continue;
        }

        const FName Bone1 = ConstraintTemplate->DefaultInstance.ConstraintBone1;
        const FName Bone2 = ConstraintTemplate->DefaultInstance.ConstraintBone2;
        if (!BodyBones.Contains(Bone1) || !BodyBones.Contains(Bone2))
        {
            Messages.Add({FVehiclePhATValidationMessage::ESeverity::Warning, FString::Printf(TEXT("Constraint '%s' references missing body."), *ConstraintTemplate->GetName())});
        }

        const FString Key = Bone1.LexicalLess(Bone2)
            ? Bone1.ToString() + TEXT("|") + Bone2.ToString()
            : Bone2.ToString() + TEXT("|") + Bone1.ToString();

        if (ConstraintKeys.Contains(Key))
        {
            Messages.Add({FVehiclePhATValidationMessage::ESeverity::Warning, FString::Printf(TEXT("Duplicated constraint between %s and %s."), *Bone1.ToString(), *Bone2.ToString())});
        }
        ConstraintKeys.Add(Key);
    }

    UE_LOG(LogVehiclePhATTools, Log, TEXT("Validated %s: %d message(s). Mirror defaults were %s -> %s."), *PhysicsAsset->GetName(), Messages.Num(), *MirrorSourcePattern, *MirrorTargetPattern);
    return Messages;
}

bool FVehiclePhATBodyUtils::CopyBodySetupProperties(const USkeletalBodySetup* SourceBody, USkeletalBodySetup* TargetBody)
{
    if (!SourceBody || !TargetBody)
    {
        return false;
    }

    const FName TargetBoneName = TargetBody->BoneName;
    const FKAggregateGeom TargetAggGeom = TargetBody->AggGeom;
    TargetBody->CopyBodyPropertiesFrom(SourceBody);
    TargetBody->BoneName = TargetBoneName;
    TargetBody->AggGeom = TargetAggGeom;
    return true;
}

bool FVehiclePhATBodyUtils::CopyBodyShapeSettings(const USkeletalBodySetup* SourceBody, USkeletalBodySetup* TargetBody, EVehiclePhATShapeMismatchPolicy Policy, FString& OutMessage)
{
    if (!SourceBody || !TargetBody)
    {
        OutMessage = TEXT("Invalid source or target body.");
        return false;
    }

    const bool bSameShapeCount = GetShapeCount(SourceBody->AggGeom) == GetShapeCount(TargetBody->AggGeom);
    if (!bSameShapeCount && Policy == EVehiclePhATShapeMismatchPolicy::SkipIncompatible)
    {
        OutMessage = TEXT("Shape count mismatch; skipped.");
        return false;
    }

    if (Policy == EVehiclePhATShapeMismatchPolicy::ReplaceShapes || bSameShapeCount)
    {
        TargetBody->AggGeom = SourceBody->AggGeom;
        OutMessage = TEXT("Shapes replaced/copied.");
        return true;
    }

    const int32 CommonBoxes = FMath::Min(SourceBody->AggGeom.BoxElems.Num(), TargetBody->AggGeom.BoxElems.Num());
    for (int32 Index = 0; Index < CommonBoxes; ++Index)
    {
        const FTransform ExistingTransform = TargetBody->AggGeom.BoxElems[Index].GetTransform();
        TargetBody->AggGeom.BoxElems[Index] = SourceBody->AggGeom.BoxElems[Index];
        TargetBody->AggGeom.BoxElems[Index].SetTransform(ExistingTransform);
    }

    const int32 CommonSpheres = FMath::Min(SourceBody->AggGeom.SphereElems.Num(), TargetBody->AggGeom.SphereElems.Num());
    for (int32 Index = 0; Index < CommonSpheres; ++Index)
    {
        const FVector ExistingCenter = TargetBody->AggGeom.SphereElems[Index].Center;
        TargetBody->AggGeom.SphereElems[Index] = SourceBody->AggGeom.SphereElems[Index];
        TargetBody->AggGeom.SphereElems[Index].Center = ExistingCenter;
    }

    const int32 CommonCapsules = FMath::Min(SourceBody->AggGeom.SphylElems.Num(), TargetBody->AggGeom.SphylElems.Num());
    for (int32 Index = 0; Index < CommonCapsules; ++Index)
    {
        const FTransform ExistingTransform = TargetBody->AggGeom.SphylElems[Index].GetTransform();
        TargetBody->AggGeom.SphylElems[Index] = SourceBody->AggGeom.SphylElems[Index];
        TargetBody->AggGeom.SphylElems[Index].SetTransform(ExistingTransform);
    }

    OutMessage = TEXT("Common primitive shape settings applied.");
    return true;
}

bool FVehiclePhATBodyUtils::CopyShapeTransforms(const USkeletalBodySetup* SourceBody, USkeletalBodySetup* TargetBody, bool bLocation, bool bRotation, bool bScaleExtent, bool bAllShapes, FString& OutMessage)
{
    if (!SourceBody || !TargetBody)
    {
        OutMessage = TEXT("Invalid source or target body.");
        return false;
    }

    auto CopyTransformFields = [bLocation, bRotation](auto& TargetElem, const auto& SourceElem)
    {
        FTransform TargetTransform = TargetElem.GetTransform();
        const FTransform SourceTransform = SourceElem.GetTransform();
        if (bLocation)
        {
            TargetTransform.SetLocation(SourceTransform.GetLocation());
        }
        if (bRotation)
        {
            TargetTransform.SetRotation(SourceTransform.GetRotation());
        }
        TargetElem.SetTransform(TargetTransform);
    };

    const int32 BoxCount = bAllShapes ? FMath::Min(SourceBody->AggGeom.BoxElems.Num(), TargetBody->AggGeom.BoxElems.Num()) : FMath::Min(1, FMath::Min(SourceBody->AggGeom.BoxElems.Num(), TargetBody->AggGeom.BoxElems.Num()));
    for (int32 Index = 0; Index < BoxCount; ++Index)
    {
        CopyTransformFields(TargetBody->AggGeom.BoxElems[Index], SourceBody->AggGeom.BoxElems[Index]);
        if (bScaleExtent)
        {
            TargetBody->AggGeom.BoxElems[Index].X = SourceBody->AggGeom.BoxElems[Index].X;
            TargetBody->AggGeom.BoxElems[Index].Y = SourceBody->AggGeom.BoxElems[Index].Y;
            TargetBody->AggGeom.BoxElems[Index].Z = SourceBody->AggGeom.BoxElems[Index].Z;
        }
    }

    const int32 SphereCount = bAllShapes ? FMath::Min(SourceBody->AggGeom.SphereElems.Num(), TargetBody->AggGeom.SphereElems.Num()) : FMath::Min(1, FMath::Min(SourceBody->AggGeom.SphereElems.Num(), TargetBody->AggGeom.SphereElems.Num()));
    for (int32 Index = 0; Index < SphereCount; ++Index)
    {
        if (bLocation)
        {
            TargetBody->AggGeom.SphereElems[Index].Center = SourceBody->AggGeom.SphereElems[Index].Center;
        }
        if (bScaleExtent)
        {
            TargetBody->AggGeom.SphereElems[Index].Radius = SourceBody->AggGeom.SphereElems[Index].Radius;
        }
    }

    const int32 CapsuleCount = bAllShapes ? FMath::Min(SourceBody->AggGeom.SphylElems.Num(), TargetBody->AggGeom.SphylElems.Num()) : FMath::Min(1, FMath::Min(SourceBody->AggGeom.SphylElems.Num(), TargetBody->AggGeom.SphylElems.Num()));
    for (int32 Index = 0; Index < CapsuleCount; ++Index)
    {
        CopyTransformFields(TargetBody->AggGeom.SphylElems[Index], SourceBody->AggGeom.SphylElems[Index]);
        if (bScaleExtent)
        {
            TargetBody->AggGeom.SphylElems[Index].Radius = SourceBody->AggGeom.SphylElems[Index].Radius;
            TargetBody->AggGeom.SphylElems[Index].Length = SourceBody->AggGeom.SphylElems[Index].Length;
        }
    }

    OutMessage = TEXT("Shape transform pasted.");
    return true;
}
