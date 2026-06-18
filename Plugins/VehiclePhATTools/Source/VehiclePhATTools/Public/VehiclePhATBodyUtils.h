#pragma once

#include "CoreMinimal.h"
#include "VehiclePhATTypes.h"

class UPhysicsAsset;
class USkeletalBodySetup;
class USkeleton;
struct FKAggregateGeom;

class VEHICLEPHATTOOLS_API FVehiclePhATBodyUtils
{
public:
    static UPhysicsAsset* GetSelectedPhysicsAsset();
    static TArray<UPhysicsAsset*> GetSelectedPhysicsAssets();
    static USkeletalBodySetup* FindBodySetup(UPhysicsAsset* PhysicsAsset, FName BoneName, int32* OutIndex = nullptr);
    static int32 FindBodySetupIndex(UPhysicsAsset* PhysicsAsset, FName BoneName);
    static bool HasAnyShape(const USkeletalBodySetup* BodySetup);
    static int32 GetShapeCount(const FKAggregateGeom& AggGeom);
    static TArray<FName> GetSkeletonBoneNames(const UPhysicsAsset* PhysicsAsset);
    static void RefreshPhysicsAsset(UPhysicsAsset* PhysicsAsset);
    static void MarkAssetChanged(UPhysicsAsset* PhysicsAsset);
    static TArray<FVehiclePhATValidationMessage> ValidatePhysicsAsset(UPhysicsAsset* PhysicsAsset, const FString& MirrorSourcePattern = TEXT("*l*"), const FString& MirrorTargetPattern = TEXT("*r*"));
    static bool CopyBodyShapeSettings(const USkeletalBodySetup* SourceBody, USkeletalBodySetup* TargetBody, EVehiclePhATShapeMismatchPolicy Policy, FString& OutMessage);
    static bool CopyBodySetupProperties(const USkeletalBodySetup* SourceBody, USkeletalBodySetup* TargetBody);
    static bool CopyShapeTransforms(const USkeletalBodySetup* SourceBody, USkeletalBodySetup* TargetBody, bool bLocation, bool bRotation, bool bScaleExtent, bool bAllShapes, FString& OutMessage);
};
