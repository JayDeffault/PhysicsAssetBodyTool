#pragma once

#include "CoreMinimal.h"

class UPhysicsAsset;
class USkeletalBodySetup;

enum class EVehiclePhATMirrorAxis : uint8
{
    X,
    Y,
    Z
};

enum class EVehiclePhATShapeMismatchPolicy : uint8
{
    ReplaceShapes,
    SkipIncompatible,
    ApplyCommonShapes
};

enum class EVehiclePhATConstraintPreset : uint8
{
    Door60,
    Door70,
    Bonnet65,
    Boot70,
    Custom
};

struct FVehiclePhATBodyPair
{
    FName SourceBone;
    FName TargetBone;
    bool bTargetBoneExists = false;
    bool bSourceBodyExists = false;
    bool bTargetBodyExists = false;
    bool bSelected = true;
    FString PreviewText;
};

struct FVehiclePhATValidationMessage
{
    enum class ESeverity : uint8 { Info, Warning, Error };
    ESeverity Severity = ESeverity::Info;
    FString Message;
};

struct FVehiclePhATMirrorOptions
{
    FString SourcePattern = TEXT("*l*");
    FString TargetPattern = TEXT("*r*");
    EVehiclePhATMirrorAxis Axis = EVehiclePhATMirrorAxis::Y;
    bool bMirrorLocation = true;
    bool bMirrorRotation = true;
    bool bMirrorShapeDimensions = false;
    bool bCopyBodyProperties = true;
    bool bReplaceExistingTargetBodies = false;
    bool bCreateMissingTargetBodies = true;
};
