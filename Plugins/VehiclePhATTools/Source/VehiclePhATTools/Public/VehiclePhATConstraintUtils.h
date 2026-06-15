#pragma once

#include "CoreMinimal.h"
#include "VehiclePhATTypes.h"

class UPhysicsAsset;
class UPhysicsConstraintTemplate;

struct FVehiclePhATConstraintOptions
{
    FName ParentBone;
    FName ChildBone;
    EVehiclePhATConstraintPreset Preset = EVehiclePhATConstraintPreset::Door70;
    float AngularLimitDegrees = 70.f;
    bool bFlipAxis = false;
    bool bUpdateExisting = false;
};

class VEHICLEPHATTOOLS_API FVehiclePhATConstraintUtils
{
public:
    static UPhysicsConstraintTemplate* FindConstraint(UPhysicsAsset* PhysicsAsset, FName BoneA, FName BoneB, int32* OutIndex = nullptr);
    static bool CreateOrUpdateConstraint(UPhysicsAsset* PhysicsAsset, const FVehiclePhATConstraintOptions& Options, FString& OutMessage);
    static float PresetDegrees(EVehiclePhATConstraintPreset Preset, float CustomDegrees);
};
