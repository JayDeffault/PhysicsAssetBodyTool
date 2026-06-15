#include "VehiclePhATClipboard.h"

#include "PhysicsEngine/SkeletalBodySetup.h"
#include "VehiclePhATBodyUtils.h"

TStrongObjectPtr<USkeletalBodySetup> FVehiclePhATClipboard::BodySettingsClipboard;
TStrongObjectPtr<USkeletalBodySetup> FVehiclePhATClipboard::TransformClipboard;

static TStrongObjectPtr<USkeletalBodySetup> CloneBodySetupForClipboard(const USkeletalBodySetup* BodySetup)
{
    if (!BodySetup)
    {
        return TStrongObjectPtr<USkeletalBodySetup>();
    }

    USkeletalBodySetup* Clone = NewObject<USkeletalBodySetup>(GetTransientPackage(), NAME_None, RF_Transient);
    Clone->CopyBodyPropertiesFrom(BodySetup);
    Clone->AggGeom = BodySetup->AggGeom;
    Clone->BoneName = BodySetup->BoneName;
    return TStrongObjectPtr<USkeletalBodySetup>(Clone);
}

bool FVehiclePhATClipboard::CopyBodySettings(const USkeletalBodySetup* BodySetup)
{
    BodySettingsClipboard = CloneBodySetupForClipboard(BodySetup);
    return BodySettingsClipboard.IsValid();
}

bool FVehiclePhATClipboard::PasteBodySettings(USkeletalBodySetup* BodySetup, FString& OutMessage)
{
    if (!BodySettingsClipboard.IsValid() || !BodySetup)
    {
        OutMessage = TEXT("Body settings clipboard is empty or target body is invalid.");
        return false;
    }

    FVehiclePhATBodyUtils::CopyBodySetupProperties(BodySettingsClipboard.Get(), BodySetup);
    return FVehiclePhATBodyUtils::CopyBodyShapeSettings(BodySettingsClipboard.Get(), BodySetup, EVehiclePhATShapeMismatchPolicy::ApplyCommonShapes, OutMessage);
}

bool FVehiclePhATClipboard::HasBodySettings()
{
    return BodySettingsClipboard.IsValid();
}

bool FVehiclePhATClipboard::CopyTransform(const USkeletalBodySetup* BodySetup)
{
    TransformClipboard = CloneBodySetupForClipboard(BodySetup);
    return TransformClipboard.IsValid();
}

bool FVehiclePhATClipboard::PasteTransform(USkeletalBodySetup* BodySetup, bool bLocation, bool bRotation, bool bScaleExtent, bool bAllShapes, FString& OutMessage)
{
    if (!TransformClipboard.IsValid() || !BodySetup)
    {
        OutMessage = TEXT("Transform clipboard is empty or target body is invalid.");
        return false;
    }

    return FVehiclePhATBodyUtils::CopyShapeTransforms(TransformClipboard.Get(), BodySetup, bLocation, bRotation, bScaleExtent, bAllShapes, OutMessage);
}

bool FVehiclePhATClipboard::HasTransform()
{
    return TransformClipboard.IsValid();
}
