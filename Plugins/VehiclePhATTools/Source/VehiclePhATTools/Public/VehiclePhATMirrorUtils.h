#pragma once

#include "CoreMinimal.h"
#include "VehiclePhATTypes.h"

class UPhysicsAsset;
class USkeletalBodySetup;
struct FKAggregateGeom;

class VEHICLEPHATTOOLS_API FVehiclePhATMirrorUtils
{
public:
    static TArray<FVehiclePhATBodyPair> BuildMirrorPairs(UPhysicsAsset* PhysicsAsset, const FVehiclePhATMirrorOptions& Options);
    static bool ApplyMirror(UPhysicsAsset* PhysicsAsset, const FVehiclePhATMirrorOptions& Options, const TArray<FVehiclePhATBodyPair>& Pairs, FString& OutMessage);
    static FString PatternToToken(const FString& Pattern);
    static FName MakeTargetName(FName SourceName, const FString& SourcePattern, const FString& TargetPattern);
    static FVector MirrorVector(const FVector& Value, EVehiclePhATMirrorAxis Axis);
    static FQuat MirrorQuat(const FQuat& Value, EVehiclePhATMirrorAxis Axis);
    static void MirrorAggGeom(FKAggregateGeom& AggGeom, const FVehiclePhATMirrorOptions& Options, const FTransform& SourceBoneToWorld, const FTransform& TargetBoneToWorld);
};
