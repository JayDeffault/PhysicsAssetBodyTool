#pragma once

#include "CoreMinimal.h"

class UPhysicsAsset;
class USkeletalBodySetup;

class VEHICLEPHATTOOLS_API FVehiclePhATConvexUtils
{
public:
    static bool AddConvexFromPoints(UPhysicsAsset* PhysicsAsset, USkeletalBodySetup* BodySetup, const TArray<FVector>& Points, FString& OutMessage);
    static bool ReplaceConvexFromPoints(UPhysicsAsset* PhysicsAsset, USkeletalBodySetup* BodySetup, int32 ConvexIndex, const TArray<FVector>& Points, FString& OutMessage);
};
