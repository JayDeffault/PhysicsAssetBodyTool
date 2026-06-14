#pragma once
#include "CoreMinimal.h"
#include "PhysicsAssetBodyToolTypes.h"

class UPhysicsAsset;
class USkeletalMesh;
class USkeletalBodySetup;
class UPhysicsConstraintTemplate;

class FPABTAssetEditor
{
public:
    static USkeletalBodySetup* FindBody(UPhysicsAsset* Asset, FName BoneName);
    static USkeletalBodySetup* EnsureBody(UPhysicsAsset* Asset, FName BoneName);
    static void FinalizeAssetChange(UPhysicsAsset* Asset);
    static void AddPrimitive(UPhysicsAsset* Asset, FName BoneName, EPABTPrimitiveType Type, float Size);
    static void DeleteBody(UPhysicsAsset* Asset, FName BoneName);
    static void DuplicateBody(UPhysicsAsset* Asset, FName SourceBone, FName TargetBone);
};

class FPABTMirrorSystem
{
public:
    FPABTMirrorSystem();
    void AddRule(const FString& A, const FString& B);
    bool FindMirrorName(FName Source, FName& OutMirror) const;
    void MirrorBody(UPhysicsAsset* Asset, const USkeletalMesh* Mesh, FName SourceBone, FName TargetBone, EPABTMirrorAxis Axis, bool bOnlyMissing) const;
    void MirrorConstraint(UPhysicsAsset* Asset, const USkeletalMesh* Mesh, const UPhysicsConstraintTemplate* SourceConstraint, FName TargetA, FName TargetB, EPABTMirrorAxis Axis) const;
    static FTransform MirrorTransformBetweenBones(const USkeletalMesh* Mesh, FName SourceBone, FName TargetBone, const FTransform& SourceLocal, EPABTMirrorAxis Axis);
private:
    TArray<FPABTMirrorNamingRule> Rules;
};

class FPABTConstraintSystem
{
public:
    static UPhysicsConstraintTemplate* CreateConstraint(UPhysicsAsset* Asset, FName ParentBone, FName ChildBone, EPABTHingePreset Preset, const FVector& HingeAxis, float MinAngle, float MaxAngle);
    static void ApplyPreset(FConstraintInstance& Instance, EPABTHingePreset Preset, const FVector& HingeAxis, float MinAngle, float MaxAngle);
};

class FPABTValidationSystem
{
public:
    static TArray<FPABTValidationIssue> Validate(UPhysicsAsset* Asset, USkeletalMesh* Mesh, const FPABTMirrorSystem& MirrorSystem);
};
