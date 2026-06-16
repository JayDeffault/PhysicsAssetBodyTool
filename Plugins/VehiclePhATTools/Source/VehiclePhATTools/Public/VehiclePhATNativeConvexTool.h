#pragma once

#include "CoreMinimal.h"

class UPhysicsAsset;
class USkeletalBodySetup;

class VEHICLEPHATTOOLS_API FVehiclePhATNativeConvexTool
{
public:
    enum class EMode : uint8
    {
        Inactive,
        Create,
        Edit
    };

    static void StartCreate(UPhysicsAsset* PhysicsAsset, FName BodyBone);
    static void StartEdit(UPhysicsAsset* PhysicsAsset, FName BodyBone, int32 ConvexIndex);
    static void Stop();

    static bool IsActive();
    static EMode GetMode();
    static UPhysicsAsset* GetPhysicsAsset();
    static FName GetBodyBone();
    static int32 GetConvexIndex();
    static const TArray<FVector>& GetPoints();

    static void SetHoverIndex(int32 Index);
    static int32 GetHoverIndex();
    static void AddPoint(const FVector& Point);
    static bool MoveHoveredPoint(const FVector& Point);
    static bool DeleteHoveredPoint();
    static bool Apply(FString& OutMessage);

private:
    static USkeletalBodySetup* GetBodySetup();
    static void LoadExistingConvex();
};
