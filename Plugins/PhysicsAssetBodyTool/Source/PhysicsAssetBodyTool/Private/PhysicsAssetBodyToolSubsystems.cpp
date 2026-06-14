#include "PhysicsAssetBodyToolSubsystems.h"
#include "Animation/Skeleton.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "ReferenceSkeleton.h"
#include "ScopedTransaction.h"

USkeletalBodySetup* FPABTAssetEditor::FindBody(UPhysicsAsset* Asset, FName BoneName)
{
    if (!Asset) return nullptr;
    for (USkeletalBodySetup* Setup : Asset->SkeletalBodySetups)
        if (Setup && Setup->BoneName == BoneName) return Setup;
    return nullptr;
}

USkeletalBodySetup* FPABTAssetEditor::EnsureBody(UPhysicsAsset* Asset, FName BoneName)
{
    if (!Asset) return nullptr;
    if (USkeletalBodySetup* Existing = FindBody(Asset, BoneName)) return Existing;
    Asset->Modify();
    USkeletalBodySetup* Setup = NewObject<USkeletalBodySetup>(Asset, NAME_None, RF_Transactional);
    Setup->BoneName = BoneName;
    Setup->CollisionTraceFlag = CTF_UseDefault;
    Asset->SkeletalBodySetups.Add(Setup);
    return Setup;
}

void FPABTAssetEditor::FinalizeAssetChange(UPhysicsAsset* Asset)
{
    if (!Asset) return;
    for (USkeletalBodySetup* Setup : Asset->SkeletalBodySetups)
    {
        if (Setup)
        {
            Setup->Modify();
            Setup->InvalidatePhysicsData();
            Setup->CreatePhysicsMeshes();
        }
    }
    Asset->UpdateBodySetupIndexMap();
    Asset->MarkPackageDirty();
}

void FPABTAssetEditor::AddPrimitive(UPhysicsAsset* Asset, FName BoneName, EPABTPrimitiveType Type, float Size)
{
    FScopedTransaction Tx(NSLOCTEXT("PhysicsAssetBodyTool", "AddPrimitive", "Add Physics Asset Primitive"));
    USkeletalBodySetup* Setup = EnsureBody(Asset, BoneName);
    if (!Setup) return;
    Setup->Modify();
    const float S = FMath::Max(Size, 0.1f);
    if (Type == EPABTPrimitiveType::Box) { FKBoxElem E; E.SetTransform(FTransform::Identity); E.X = E.Y = E.Z = S; Setup->AggGeom.BoxElems.Add(E); }
    else if (Type == EPABTPrimitiveType::Sphere) { FKSphereElem E; E.SetTransform(FTransform::Identity); E.Radius = S * .5f; Setup->AggGeom.SphereElems.Add(E); }
    else if (Type == EPABTPrimitiveType::Capsule) { FKSphylElem E; E.SetTransform(FTransform::Identity); E.Radius = S * .25f; E.Length = S; Setup->AggGeom.SphylElems.Add(E); }
    else { FKConvexElem E; E.VertexData = { FVector(-S,-S,-S), FVector(S,-S,-S), FVector(S,S,-S), FVector(-S,S,-S), FVector(-S,-S,S), FVector(S,-S,S), FVector(S,S,S), FVector(-S,S,S) }; E.UpdateElemBox(); Setup->AggGeom.ConvexElems.Add(E); }
    FinalizeAssetChange(Asset);
}

void FPABTAssetEditor::DeleteBody(UPhysicsAsset* Asset, FName BoneName)
{
    if (!Asset) return;
    FScopedTransaction Tx(NSLOCTEXT("PhysicsAssetBodyTool", "DeleteBody", "Delete Physics Asset Body"));
    Asset->Modify();
    Asset->SkeletalBodySetups.RemoveAll([BoneName](const USkeletalBodySetup* S){ return S && S->BoneName == BoneName; });
    FinalizeAssetChange(Asset);
}


void FPABTAssetEditor::SetBodyPhysicsType(UPhysicsAsset* Asset, FName BoneName, EPhysicsType NewPhysicsType)
{
    USkeletalBodySetup* Setup = FindBody(Asset, BoneName);
    if (!Asset || !Setup) return;

    FScopedTransaction Tx(NSLOCTEXT("PhysicsAssetBodyTool", "SetBodyPhysicsType", "Set Physics Body Simulation Type"));
    Asset->Modify();
    Setup->Modify();
    Setup->DefaultInstance.SetInstanceSimulatePhysics(NewPhysicsType == PhysType_Simulated);
    FinalizeAssetChange(Asset);
}

void FPABTAssetEditor::DuplicateBody(UPhysicsAsset* Asset, FName SourceBone, FName TargetBone)
{
    USkeletalBodySetup* Source = FindBody(Asset, SourceBone); if (!Source || !Asset) return;
    FScopedTransaction Tx(NSLOCTEXT("PhysicsAssetBodyTool", "DuplicateBody", "Duplicate Physics Asset Body"));
    Asset->Modify();
    USkeletalBodySetup* Copy = DuplicateObject<USkeletalBodySetup>(Source, Asset);
    Copy->SetFlags(RF_Transactional); Copy->BoneName = TargetBone;
    Asset->SkeletalBodySetups.RemoveAll([TargetBone](const USkeletalBodySetup* S){ return S && S->BoneName == TargetBone; });
    Asset->SkeletalBodySetups.Add(Copy);
    FinalizeAssetChange(Asset);
}

FPABTMirrorSystem::FPABTMirrorSystem()
{
    AddRule(TEXT("_L"), TEXT("_R")); AddRule(TEXT("_l"), TEXT("_r")); AddRule(TEXT(".L"), TEXT(".R")); AddRule(TEXT(".l"), TEXT(".r")); AddRule(TEXT("Left"), TEXT("Right")); AddRule(TEXT("left"), TEXT("right"));
}
void FPABTMirrorSystem::AddRule(const FString& A, const FString& B) { Rules.Emplace(A, B); }
bool FPABTMirrorSystem::FindMirrorName(FName Source, FName& OutMirror) const
{
    const FString S = Source.ToString();
    for (const FPABTMirrorNamingRule& R : Rules)
    { if (S.Contains(R.A)) { OutMirror = FName(*S.Replace(*R.A, *R.B)); return true; } if (S.Contains(R.B)) { OutMirror = FName(*S.Replace(*R.B, *R.A)); return true; } }
    return false;
}

static FTransform RefPoseComponentTransform(const USkeletalMesh* Mesh, FName Bone)
{
    const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton(); int32 Index = Ref.FindBoneIndex(Bone); FTransform T = FTransform::Identity;
    while (Index != INDEX_NONE) { T = T * Ref.GetRefBonePose()[Index]; Index = Ref.GetParentIndex(Index); }
    return T;
}

FTransform FPABTMirrorSystem::MirrorTransformBetweenBones(const USkeletalMesh* Mesh, FName SourceBone, FName TargetBone, const FTransform& SourceLocal, EPABTMirrorAxis Axis)
{
    if (!Mesh) return SourceLocal;
    FTransform Comp = SourceLocal * RefPoseComponentTransform(Mesh, SourceBone);
    FVector Loc = Comp.GetLocation();
    FVector Scl = Comp.GetScale3D();
    if (Axis == EPABTMirrorAxis::X) Loc.X *= -1.f; else if (Axis == EPABTMirrorAxis::Y) Loc.Y *= -1.f; else Loc.Z *= -1.f;
    FMatrix M = Comp.ToMatrixWithScale();
    const FVector N = Axis == EPABTMirrorAxis::X ? FVector::XAxisVector : Axis == EPABTMirrorAxis::Y ? FVector::YAxisVector : FVector::ZAxisVector;
    FMatrix Mirror = FScaleMatrix(FVector(1,1,1) - 2.f * N);
    FTransform Mirrored(Mirror * M * Mirror); Mirrored.SetLocation(Loc); Mirrored.SetScale3D(Scl);
    return Mirrored.GetRelativeTransform(RefPoseComponentTransform(Mesh, TargetBone));
}

void FPABTMirrorSystem::MirrorBody(UPhysicsAsset* Asset, const USkeletalMesh* Mesh, FName SourceBone, FName TargetBone, EPABTMirrorAxis Axis, bool bOnlyMissing) const
{
    USkeletalBodySetup* Source = FPABTAssetEditor::FindBody(Asset, SourceBone); if (!Source || !Asset || !Mesh) return;
    if (bOnlyMissing && FPABTAssetEditor::FindBody(Asset, TargetBone)) return;
    FScopedTransaction Tx(NSLOCTEXT("PhysicsAssetBodyTool", "MirrorBody", "Mirror Physics Asset Body"));
    FPABTAssetEditor::DuplicateBody(Asset, SourceBone, TargetBone);
    USkeletalBodySetup* Target = FPABTAssetEditor::FindBody(Asset, TargetBone); if (!Target) return;
    Target->Modify();
    for (FKBoxElem& E : Target->AggGeom.BoxElems) E.SetTransform(MirrorTransformBetweenBones(Mesh, SourceBone, TargetBone, E.GetTransform(), Axis));
    for (FKSphereElem& E : Target->AggGeom.SphereElems) E.SetTransform(MirrorTransformBetweenBones(Mesh, SourceBone, TargetBone, E.GetTransform(), Axis));
    for (FKSphylElem& E : Target->AggGeom.SphylElems) E.SetTransform(MirrorTransformBetweenBones(Mesh, SourceBone, TargetBone, E.GetTransform(), Axis));
    for (FKConvexElem& E : Target->AggGeom.ConvexElems) { E.SetTransform(MirrorTransformBetweenBones(Mesh, SourceBone, TargetBone, E.GetTransform(), Axis)); E.UpdateElemBox(); }
    FPABTAssetEditor::FinalizeAssetChange(Asset);
}

void FPABTMirrorSystem::MirrorConstraint(UPhysicsAsset* Asset, const USkeletalMesh* Mesh, const UPhysicsConstraintTemplate* SourceConstraint, FName TargetA, FName TargetB, EPABTMirrorAxis Axis) const
{
    if (!Asset || !SourceConstraint) return;
    FScopedTransaction Tx(NSLOCTEXT("PhysicsAssetBodyTool", "MirrorConstraint", "Mirror Physics Asset Constraint"));
    Asset->Modify();
    UPhysicsConstraintTemplate* C = DuplicateObject<UPhysicsConstraintTemplate>(SourceConstraint, Asset);
    C->SetFlags(RF_Transactional); C->DefaultInstance.ConstraintBone1 = TargetA; C->DefaultInstance.ConstraintBone2 = TargetB;
    C->DefaultInstance.Pos1 *= -1.f; C->DefaultInstance.Pos2 *= -1.f;
    Asset->ConstraintSetup.Add(C); Asset->MarkPackageDirty();
}

void FPABTConstraintSystem::ApplyPreset(FConstraintInstance& I, EPABTHingePreset Preset, const FVector& HingeAxis, float MinAngle, float MaxAngle)
{
    I.SetDisableCollision(true); I.SetLinearXLimit(LCM_Locked, 0); I.SetLinearYLimit(LCM_Locked, 0); I.SetLinearZLimit(LCM_Locked, 0);
    if (Preset == EPABTHingePreset::Locked) { I.SetAngularSwing1Limit(ACM_Locked, 0); I.SetAngularSwing2Limit(ACM_Locked, 0); I.SetAngularTwistLimit(ACM_Locked, 0); return; }
    if (Preset == EPABTHingePreset::Limited) { I.SetAngularSwing1Limit(ACM_Limited, MaxAngle); I.SetAngularSwing2Limit(ACM_Limited, MaxAngle); I.SetAngularTwistLimit(ACM_Limited, MaxAngle); return; }
    I.SetAngularSwing1Limit(ACM_Limited, FMath::Abs(MaxAngle - MinAngle)); I.SetAngularSwing2Limit(ACM_Locked, 0); I.SetAngularTwistLimit(ACM_Locked, 0);
    I.ProfileInstance.AngularDrive.AngularDriveMode = EAngularDriveMode::TwistAndSwing;
}
UPhysicsConstraintTemplate* FPABTConstraintSystem::CreateConstraint(UPhysicsAsset* Asset, FName ParentBone, FName ChildBone, EPABTHingePreset Preset, const FVector& HingeAxis, float MinAngle, float MaxAngle)
{
    if (!Asset) return nullptr; FScopedTransaction Tx(NSLOCTEXT("PhysicsAssetBodyTool", "CreateConstraint", "Create Physics Asset Constraint")); Asset->Modify();
    UPhysicsConstraintTemplate* C = NewObject<UPhysicsConstraintTemplate>(Asset, NAME_None, RF_Transactional); C->DefaultInstance.ConstraintBone1 = ParentBone; C->DefaultInstance.ConstraintBone2 = ChildBone; ApplyPreset(C->DefaultInstance, Preset, HingeAxis, MinAngle, MaxAngle); Asset->ConstraintSetup.Add(C); Asset->MarkPackageDirty(); return C;
}

TArray<FPABTValidationIssue> FPABTValidationSystem::Validate(UPhysicsAsset* Asset, USkeletalMesh* Mesh, const FPABTMirrorSystem& MirrorSystem)
{
    TArray<FPABTValidationIssue> Out; if (!Asset) return Out;
    TSet<FName> Bodies;
    for (USkeletalBodySetup* S : Asset->SkeletalBodySetups)
    {
        if (!S) continue; if (Bodies.Contains(S->BoneName)) Out.Add({FPABTValidationIssue::ESeverity::Error, FText::Format(NSLOCTEXT("PhysicsAssetBodyTool", "DuplicateBodyIssue", "Duplicate body on {0}"), FText::FromName(S->BoneName)), S->BoneName});
        Bodies.Add(S->BoneName); const bool bEmpty = S->AggGeom.BoxElems.Num()+S->AggGeom.SphereElems.Num()+S->AggGeom.SphylElems.Num()+S->AggGeom.ConvexElems.Num()==0;
        if (bEmpty) Out.Add({FPABTValidationIssue::ESeverity::Warning, FText::Format(NSLOCTEXT("PhysicsAssetBodyTool", "EmptyBodyIssue", "Empty body on {0}"), FText::FromName(S->BoneName)), S->BoneName});
        FName Mirror; if (MirrorSystem.FindMirrorName(S->BoneName, Mirror) && !Bodies.Contains(Mirror) && !FPABTAssetEditor::FindBody(Asset, Mirror)) Out.Add({FPABTValidationIssue::ESeverity::Warning, FText::Format(NSLOCTEXT("PhysicsAssetBodyTool", "MissingMirrorIssue", "Missing mirrored body {0}"), FText::FromName(Mirror)), S->BoneName});
    }
    for (UPhysicsConstraintTemplate* C : Asset->ConstraintSetup) if (C && (!Bodies.Contains(C->DefaultInstance.ConstraintBone1) || !Bodies.Contains(C->DefaultInstance.ConstraintBone2))) Out.Add({FPABTValidationIssue::ESeverity::Error, NSLOCTEXT("PhysicsAssetBodyTool", "BrokenConstraintIssue", "Constraint references missing body"), C->DefaultInstance.ConstraintBone2});
    return Out;
}
