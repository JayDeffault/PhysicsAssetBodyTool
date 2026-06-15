#include "VehiclePhATClipboard.h"
#include "VehiclePhATBodyUtils.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

USkeletalBodySetup* FVehiclePhATClipboard::BodySettingsClipboard=nullptr;
USkeletalBodySetup* FVehiclePhATClipboard::TransformClipboard=nullptr;

static USkeletalBodySetup* CloneBody(const USkeletalBodySetup* B){ if(!B)return nullptr; USkeletalBodySetup* C=NewObject<USkeletalBodySetup>(GetTransientPackage()); C->AddToRoot(); C->CopyBodyPropertiesFrom(B); C->AggGeom=B->AggGeom; C->BoneName=B->BoneName; return C; }
bool FVehiclePhATClipboard::CopyBodySettings(const USkeletalBodySetup* B){ BodySettingsClipboard=CloneBody(B); return BodySettingsClipboard!=nullptr;}
bool FVehiclePhATClipboard::PasteBodySettings(USkeletalBodySetup* B,FString& M){ if(!BodySettingsClipboard||!B)return false; FVehiclePhATBodyUtils::CopyBodySetupProperties(BodySettingsClipboard,B); return FVehiclePhATBodyUtils::CopyBodyShapeSettings(BodySettingsClipboard,B,EVehiclePhATShapeMismatchPolicy::ApplyCommonShapes,M);}
bool FVehiclePhATClipboard::HasBodySettings(){return BodySettingsClipboard!=nullptr;}
bool FVehiclePhATClipboard::CopyTransform(const USkeletalBodySetup* B){ TransformClipboard=CloneBody(B); return TransformClipboard!=nullptr;}
bool FVehiclePhATClipboard::PasteTransform(USkeletalBodySetup* B,bool L,bool R,bool E,bool All,FString& M){ return TransformClipboard&&B&&FVehiclePhATBodyUtils::CopyShapeTransforms(TransformClipboard,B,L,R,E,All,M);}
bool FVehiclePhATClipboard::HasTransform(){return TransformClipboard!=nullptr;}
