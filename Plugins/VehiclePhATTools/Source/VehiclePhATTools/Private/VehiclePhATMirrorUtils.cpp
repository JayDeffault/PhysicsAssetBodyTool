#include "VehiclePhATMirrorUtils.h"

#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "ScopedTransaction.h"
#include "VehiclePhATBodyUtils.h"
#include "VehiclePhATToolsLog.h"

FString FVehiclePhATMirrorUtils::PatternToToken(const FString& Pattern)
{
    FString Token = Pattern;
    Token.ReplaceInline(TEXT("*"), TEXT(""));
    return Token;
}

FName FVehiclePhATMirrorUtils::MakeTargetName(FName SourceName, const FString& SourcePattern, const FString& TargetPattern)
{
    FString TargetName = SourceName.ToString();
    TargetName.ReplaceInline(*PatternToToken(SourcePattern), *PatternToToken(TargetPattern), ESearchCase::IgnoreCase);
    return FName(*TargetName);
}

FVector FVehiclePhATMirrorUtils::MirrorVector(const FVector& Value, EVehiclePhATMirrorAxis Axis)
{
    FVector Result = Value;
    if (Axis == EVehiclePhATMirrorAxis::X)
    {
        Result.X *= -1.f;
    }
    else if (Axis == EVehiclePhATMirrorAxis::Y)
    {
        Result.Y *= -1.f;
    }
    else
    {
        Result.Z *= -1.f;
    }
    return Result;
}

FQuat FVehiclePhATMirrorUtils::MirrorQuat(const FQuat& Value, EVehiclePhATMirrorAxis Axis)
{
    const FMatrix RotationMatrix = FQuatRotationMatrix(Value);
    FVector XAxis = MirrorVector(RotationMatrix.GetScaledAxis(EAxis::X), Axis);
    FVector ZAxis = MirrorVector(RotationMatrix.GetScaledAxis(EAxis::Z), Axis);

    // Mirroring one axis changes handedness. Flip the mirrored basis vector back so the resulting
    // matrix remains a valid right-handed rotation rather than a reflection matrix.
    if (Axis == EVehiclePhATMirrorAxis::X)
    {
        XAxis *= -1.f;
    }
    else if (Axis == EVehiclePhATMirrorAxis::Z)
    {
        ZAxis *= -1.f;
    }
    else
    {
        XAxis *= -1.f;
    }

    return FRotationMatrix::MakeFromXZ(XAxis, ZAxis).ToQuat();
}

void FVehiclePhATMirrorUtils::MirrorAggGeom(FKAggregateGeom& AggGeom, const FVehiclePhATMirrorOptions& Options)
{
    auto MirrorTransform = [&Options](FTransform Transform)
    {
        if (Options.bMirrorLocation)
        {
            Transform.SetLocation(MirrorVector(Transform.GetLocation(), Options.Axis));
        }
        if (Options.bMirrorRotation)
        {
            Transform.SetRotation(MirrorQuat(Transform.GetRotation(), Options.Axis));
        }
        return Transform;
    };

    for (FKBoxElem& Box : AggGeom.BoxElems)
    {
        Box.SetTransform(MirrorTransform(Box.GetTransform()));
    }

    for (FKSphylElem& Capsule : AggGeom.SphylElems)
    {
        Capsule.SetTransform(MirrorTransform(Capsule.GetTransform()));
    }

    for (FKTaperedCapsuleElem& TaperedCapsule : AggGeom.TaperedCapsuleElems)
    {
        TaperedCapsule.SetTransform(MirrorTransform(TaperedCapsule.GetTransform()));
    }

    for (FKSphereElem& Sphere : AggGeom.SphereElems)
    {
        if (Options.bMirrorLocation)
        {
            Sphere.Center = MirrorVector(Sphere.Center, Options.Axis);
        }
    }

    for (FKConvexElem& Convex : AggGeom.ConvexElems)
    {
        Convex.SetTransform(MirrorTransform(Convex.GetTransform()));
        if (Options.bMirrorLocation)
        {
            for (FVector& Vertex : Convex.VertexData)
            {
                Vertex = MirrorVector(Vertex, Options.Axis);
            }
        }
        Convex.UpdateElemBox();
    }
}

TArray<FVehiclePhATBodyPair> FVehiclePhATMirrorUtils::BuildMirrorPairs(UPhysicsAsset* PhysicsAsset, const FVehiclePhATMirrorOptions& Options)
{
    TArray<FVehiclePhATBodyPair> Result;
    if (!PhysicsAsset)
    {
        return Result;
    }

    const FString SourceToken = PatternToToken(Options.SourcePattern);
    const TArray<FName> SkeletonBones = FVehiclePhATBodyUtils::GetSkeletonBoneNames(PhysicsAsset);
    TSet<FName> SkeletonBoneSet;
    for (FName BoneName : SkeletonBones)
    {
        SkeletonBoneSet.Add(BoneName);
    }

    for (FName SourceBone : SkeletonBones)
    {
        if (!SourceBone.ToString().Contains(SourceToken, ESearchCase::IgnoreCase))
        {
            continue;
        }

        FVehiclePhATBodyPair Pair;
        Pair.SourceBone = SourceBone;
        Pair.TargetBone = MakeTargetName(SourceBone, Options.SourcePattern, Options.TargetPattern);
        Pair.bSourceBodyExists = FVehiclePhATBodyUtils::FindBodySetup(PhysicsAsset, SourceBone) != nullptr;
        Pair.bTargetBodyExists = FVehiclePhATBodyUtils::FindBodySetup(PhysicsAsset, Pair.TargetBone) != nullptr;
        Pair.bTargetBoneExists = SkeletonBoneSet.Contains(Pair.TargetBone);
        Pair.PreviewText = FString::Printf(TEXT("%s -> %s"), *SourceBone.ToString(), *Pair.TargetBone.ToString());
        Pair.bSelected = Pair.bSourceBodyExists && Pair.bTargetBoneExists && Pair.SourceBone != Pair.TargetBone;
        Result.Add(Pair);
    }

    return Result;
}

bool FVehiclePhATMirrorUtils::ApplyMirror(UPhysicsAsset* PhysicsAsset, const FVehiclePhATMirrorOptions& Options, const TArray<FVehiclePhATBodyPair>& Pairs, FString& OutMessage)
{
    if (!PhysicsAsset)
    {
        OutMessage = TEXT("No PhysicsAsset selected.");
        return false;
    }

    FScopedTransaction Transaction(NSLOCTEXT("VehiclePhATTools", "MirrorBodies", "Mirror Vehicle Physics Bodies"));
    PhysicsAsset->Modify();

    int32 MirroredCount = 0;
    int32 SkippedCount = 0;
    for (const FVehiclePhATBodyPair& Pair : Pairs)
    {
        if (!Pair.bSelected)
        {
            ++SkippedCount;
            continue;
        }

        USkeletalBodySetup* SourceBody = FVehiclePhATBodyUtils::FindBodySetup(PhysicsAsset, Pair.SourceBone);
        if (!SourceBody)
        {
            UE_LOG(LogVehiclePhATTools, Warning, TEXT("Cannot mirror %s: source body not found."), *Pair.SourceBone.ToString());
            ++SkippedCount;
            continue;
        }

        USkeletalBodySetup* TargetBody = FVehiclePhATBodyUtils::FindBodySetup(PhysicsAsset, Pair.TargetBone);
        if (TargetBody && !Options.bReplaceExistingTargetBodies)
        {
            UE_LOG(LogVehiclePhATTools, Warning, TEXT("Skipping %s: target body already exists."), *Pair.TargetBone.ToString());
            ++SkippedCount;
            continue;
        }

        if (!TargetBody && Options.bCreateMissingTargetBodies)
        {
            TargetBody = NewObject<USkeletalBodySetup>(PhysicsAsset, NAME_None, RF_Transactional);
            PhysicsAsset->SkeletalBodySetups.Add(TargetBody);
        }

        if (!TargetBody)
        {
            ++SkippedCount;
            continue;
        }

        TargetBody->Modify();
        TargetBody->CopyBodyPropertiesFrom(SourceBody);
        TargetBody->BoneName = Pair.TargetBone;
        TargetBody->AggGeom = SourceBody->AggGeom;
        MirrorAggGeom(TargetBody->AggGeom, Options);
        ++MirroredCount;
    }

    FVehiclePhATBodyUtils::MarkAssetChanged(PhysicsAsset);
    OutMessage = FString::Printf(TEXT("Mirrored %d body/bodies. Skipped %d."), MirroredCount, SkippedCount);
    return MirroredCount > 0;
}
