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

    enum class EViewportAction : uint8
    {
        Hover,
        PrimaryPress,
        PrimaryDrag,
        SecondaryPress
    };

    struct FViewportPoint
    {
        FVector Position = FVector::ZeroVector;
        FLinearColor Color = FLinearColor::White;
        float Size = 8.f;
        bool bHovered = false;
    };

    struct FViewportSegment
    {
        FVector Start = FVector::ZeroVector;
        FVector End = FVector::ZeroVector;
        FLinearColor Color = FLinearColor::White;
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

    // Native PhAT viewport bridge:
    // - call SelectNearestPointToRay / SelectHoveredPoint from viewport hit-testing;
    // - call GetSelectedPointTransform from the viewport client's widget-location path;
    // - call ApplySelectedPointDelta from the viewport client's transform-gizmo delta path.
    static bool HasSelectedPoint();
    static int32 GetSelectedPointIndex();
    static bool SelectHoveredPoint();
    static bool SelectNearestPointToRay(const FVector& RayOrigin, const FVector& RayDirection, float PixelWorldTolerance, float MaxRayDistance);
    static bool GetSelectedPointTransform(FTransform& OutTransform);
    static bool MoveSelectedPoint(const FVector& NewPosition, bool bSnapToMesh, float MaxSnapDistance);
    static bool ApplySelectedPointDelta(const FVector& Delta, bool bSnapToMesh, float MaxSnapDistance);
    static void AddPoint(const FVector& Point);
    static void AddPointSnappedToMesh(const FVector& Point, float MaxSnapDistance);
    static bool MoveHoveredPoint(const FVector& Point);
    static bool MoveHoveredPointSnappedToMesh(const FVector& Point, float MaxSnapDistance);
    static bool DeleteHoveredPoint();
    static bool SnapPointToNearestPreviewMeshVertex(const FVector& Point, float MaxSnapDistance, FVector& OutSnappedPoint);
    static bool FindNearestPreviewMeshVertexToRay(const FVector& RayOrigin, const FVector& RayDirection, float MaxRayDistance, FVector& OutSnappedPoint, int32& OutVertexIndex);
    static bool UpdateHoverFromRay(const FVector& RayOrigin, const FVector& RayDirection, float MaxRayDistance);
    static bool AddPointFromRay(const FVector& RayOrigin, const FVector& RayDirection, float MaxRayDistance);
    static bool MoveHoveredPointFromRay(const FVector& RayOrigin, const FVector& RayDirection, float MaxRayDistance);
    static bool HandleViewportRayAction(EViewportAction Action, const FVector& RayOrigin, const FVector& RayDirection, float MaxRayDistance, FString& OutMessage);
    static void BuildViewportRenderData(TArray<FViewportPoint>& OutPoints, TArray<FViewportSegment>& OutSegments);
    static bool Apply(FString& OutMessage);

private:
    static USkeletalBodySetup* GetBodySetup();
    static void LoadExistingConvex();
};
