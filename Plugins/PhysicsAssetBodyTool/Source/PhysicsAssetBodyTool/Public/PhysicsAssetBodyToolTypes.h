#pragma once
#include "CoreMinimal.h"
#include "PhysicsEngine/ConstraintInstance.h"

enum class EPABTPrimitiveType : uint8 { Box, Sphere, Capsule, Convex };
enum class EPABTMirrorAxis : uint8 { X, Y, Z };
enum class EPABTHingePreset : uint8 { VehicleDoor, Hood, Trunk, GenericHinge, Locked, Limited, WheelSuspension };

struct FPABTMirrorNamingRule
{
    FString A;
    FString B;
    FPABTMirrorNamingRule() = default;
    FPABTMirrorNamingRule(const FString& InA, const FString& InB) : A(InA), B(InB) {}
};

struct FPABTBodyTemplate
{
    FName Name;
    TArray<FKBoxElem> Boxes;
    TArray<FKSphereElem> Spheres;
    TArray<FKSphylElem> Capsules;
    TArray<FKConvexElem> Convexes;
};

struct FPABTConstraintTemplate
{
    FName Name;
    EPABTHingePreset Preset = EPABTHingePreset::GenericHinge;
    FVector HingeAxis = FVector::UpVector;
    float MinAngle = 0.f;
    float MaxAngle = 70.f;
    FConstraintInstance Instance;
};

struct FPABTValidationIssue
{
    enum class ESeverity : uint8 { Info, Warning, Error };
    ESeverity Severity = ESeverity::Warning;
    FText Message;
    FName BoneName;
    int32 ObjectIndex = INDEX_NONE;
    TFunction<void()> Fix;
};
