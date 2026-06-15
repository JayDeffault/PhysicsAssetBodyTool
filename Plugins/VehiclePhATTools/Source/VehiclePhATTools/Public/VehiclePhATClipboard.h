#pragma once

#include "CoreMinimal.h"

class USkeletalBodySetup;

class VEHICLEPHATTOOLS_API FVehiclePhATClipboard
{
public:
    static bool CopyBodySettings(const USkeletalBodySetup* BodySetup);
    static bool PasteBodySettings(USkeletalBodySetup* BodySetup, FString& OutMessage);
    static bool HasBodySettings();
    static bool CopyTransform(const USkeletalBodySetup* BodySetup);
    static bool PasteTransform(USkeletalBodySetup* BodySetup, bool bLocation, bool bRotation, bool bScaleExtent, bool bAllShapes, FString& OutMessage);
    static bool HasTransform();

private:
    static USkeletalBodySetup* BodySettingsClipboard;
    static USkeletalBodySetup* TransformClipboard;
};
