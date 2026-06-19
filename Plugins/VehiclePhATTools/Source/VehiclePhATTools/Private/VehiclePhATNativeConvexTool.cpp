#include "VehiclePhATNativeConvexTool.h"

#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "ScopedTransaction.h"
#include "VehiclePhATBodyUtils.h"
#include "VehiclePhATConvexUtils.h"
#include "VehiclePhATToolsLog.h"

namespace VehiclePhATNativeConvexToolState
{
FVehiclePhATNativeConvexTool::EMode Mode = FVehiclePhATNativeConvexTool::EMode::Inactive;
TWeakObjectPtr<UPhysicsAsset> PhysicsAsset;
FName BodyBone = NAME_None;
int32 ConvexIndex = INDEX_NONE;
int32 HoverIndex = INDEX_NONE;
int32 SelectedIndex = INDEX_NONE;
int32 MarkerStartIndex = INDEX_NONE;
int32 MarkerCount = 0;
int32 MarkerAllocatedCount = 0;
TArray<FVector> Points;
TArray<FVector> LastLiveUpdatePoints;
TArray<FVector> LastObservedMarkerPoints;
double LastObservedMarkerChangeTime = 0.0;
bool bPendingDebouncedUpdate = false;
bool bLiveCreatedConvex = false;
}

namespace
{
bool ArePointArraysNearlyEqual(const TArray<FVector>& A, const TArray<FVector>& B, float Tolerance)
{
    if (A.Num() != B.Num())
    {
        return false;
    }

    for (int32 PointIndex = 0; PointIndex < A.Num(); ++PointIndex)
    {
        if (!A[PointIndex].Equals(B[PointIndex], Tolerance))
        {
            return false;
        }
    }

    return true;
}

void RemoveUnappliedLiveCreatedConvex()
{
    using namespace VehiclePhATNativeConvexToolState;
    UPhysicsAsset* ActivePhysicsAsset = PhysicsAsset.Get();
    USkeletalBodySetup* BodySetup = FVehiclePhATBodyUtils::FindBodySetup(ActivePhysicsAsset, BodyBone);
    if (!bLiveCreatedConvex || !ActivePhysicsAsset || !BodySetup || !BodySetup->AggGeom.ConvexElems.IsValidIndex(ConvexIndex))
    {
        bLiveCreatedConvex = false;
        return;
    }

    ActivePhysicsAsset->Modify();
    BodySetup->Modify();
    BodySetup->AggGeom.ConvexElems.RemoveAt(ConvexIndex);
    FVehiclePhATBodyUtils::MarkBodySetupGeometryChanged(ActivePhysicsAsset, BodySetup);
    bLiveCreatedConvex = false;
}
}

void FVehiclePhATNativeConvexTool::StartCreate(UPhysicsAsset* PhysicsAsset, FName BodyBone)
{
    using namespace VehiclePhATNativeConvexToolState;
    RemoveUnappliedLiveCreatedConvex();
    Mode = EMode::Create;
    VehiclePhATNativeConvexToolState::PhysicsAsset = PhysicsAsset;
    VehiclePhATNativeConvexToolState::BodyBone = BodyBone;
    ConvexIndex = INDEX_NONE;
    HoverIndex = INDEX_NONE;
    SelectedIndex = INDEX_NONE;
    MarkerStartIndex = INDEX_NONE;
    MarkerCount = 0;
    MarkerAllocatedCount = 0;
    Points.Reset();
    LastLiveUpdatePoints.Reset();
    LastObservedMarkerPoints.Reset();
    LastObservedMarkerChangeTime = 0.0;
    bPendingDebouncedUpdate = false;
    bLiveCreatedConvex = false;
    UE_LOG(LogVehiclePhATTools, Log, TEXT("Native convex create mode started for body '%s'."), *BodyBone.ToString());
}

void FVehiclePhATNativeConvexTool::StartEdit(UPhysicsAsset* PhysicsAsset, FName BodyBone, int32 InConvexIndex)
{
    using namespace VehiclePhATNativeConvexToolState;
    RemoveUnappliedLiveCreatedConvex();
    Mode = EMode::Edit;
    VehiclePhATNativeConvexToolState::PhysicsAsset = PhysicsAsset;
    VehiclePhATNativeConvexToolState::BodyBone = BodyBone;
    ConvexIndex = InConvexIndex;
    HoverIndex = INDEX_NONE;
    SelectedIndex = INDEX_NONE;
    MarkerStartIndex = INDEX_NONE;
    MarkerCount = 0;
    MarkerAllocatedCount = 0;
    Points.Reset();
    LastLiveUpdatePoints.Reset();
    LastObservedMarkerPoints.Reset();
    LastObservedMarkerChangeTime = 0.0;
    bPendingDebouncedUpdate = false;
    bLiveCreatedConvex = false;
    LoadExistingConvex();
    UE_LOG(LogVehiclePhATTools, Log, TEXT("Native convex edit mode started for body '%s' convex %d."), *BodyBone.ToString(), InConvexIndex);
}

void FVehiclePhATNativeConvexTool::Stop()
{
    using namespace VehiclePhATNativeConvexToolState;
    RemoveUnappliedLiveCreatedConvex();
    Mode = EMode::Inactive;
    PhysicsAsset.Reset();
    BodyBone = NAME_None;
    ConvexIndex = INDEX_NONE;
    HoverIndex = INDEX_NONE;
    SelectedIndex = INDEX_NONE;
    MarkerStartIndex = INDEX_NONE;
    MarkerCount = 0;
    MarkerAllocatedCount = 0;
    Points.Reset();
    LastLiveUpdatePoints.Reset();
    LastObservedMarkerPoints.Reset();
    LastObservedMarkerChangeTime = 0.0;
    bPendingDebouncedUpdate = false;
    bLiveCreatedConvex = false;
}

bool FVehiclePhATNativeConvexTool::IsActive()
{
    return VehiclePhATNativeConvexToolState::Mode != EMode::Inactive;
}

FVehiclePhATNativeConvexTool::EMode FVehiclePhATNativeConvexTool::GetMode()
{
    return VehiclePhATNativeConvexToolState::Mode;
}

UPhysicsAsset* FVehiclePhATNativeConvexTool::GetPhysicsAsset()
{
    return VehiclePhATNativeConvexToolState::PhysicsAsset.Get();
}

FName FVehiclePhATNativeConvexTool::GetBodyBone()
{
    return VehiclePhATNativeConvexToolState::BodyBone;
}

int32 FVehiclePhATNativeConvexTool::GetConvexIndex()
{
    return VehiclePhATNativeConvexToolState::ConvexIndex;
}

const TArray<FVector>& FVehiclePhATNativeConvexTool::GetPoints()
{
    return VehiclePhATNativeConvexToolState::Points;
}

void FVehiclePhATNativeConvexTool::SetPoints(const TArray<FVector>& InPoints)
{
    using namespace VehiclePhATNativeConvexToolState;
    Points = InPoints;
    HoverIndex = INDEX_NONE;
    SelectedIndex = Points.Num() > 0 ? 0 : INDEX_NONE;
}

void FVehiclePhATNativeConvexTool::SetHoverIndex(int32 Index)
{
    VehiclePhATNativeConvexToolState::HoverIndex = Index;
}

int32 FVehiclePhATNativeConvexTool::GetHoverIndex()
{
    return VehiclePhATNativeConvexToolState::HoverIndex;
}

bool FVehiclePhATNativeConvexTool::HasSelectedPoint()
{
    using namespace VehiclePhATNativeConvexToolState;
    return Points.IsValidIndex(SelectedIndex);
}

int32 FVehiclePhATNativeConvexTool::GetSelectedPointIndex()
{
    return VehiclePhATNativeConvexToolState::SelectedIndex;
}

bool FVehiclePhATNativeConvexTool::SelectHoveredPoint()
{
    using namespace VehiclePhATNativeConvexToolState;
    if (!Points.IsValidIndex(HoverIndex))
    {
        SelectedIndex = INDEX_NONE;
        return false;
    }

    SelectedIndex = HoverIndex;
    return true;
}

bool FVehiclePhATNativeConvexTool::SelectNearestPointToRay(const FVector& RayOrigin, const FVector& RayDirection, float PixelWorldTolerance, float MaxRayDistance)
{
    using namespace VehiclePhATNativeConvexToolState;
    const FVector SafeRayDirection = RayDirection.GetSafeNormal();
    if (!IsActive() || SafeRayDirection.IsNearlyZero())
    {
        SelectedIndex = INDEX_NONE;
        HoverIndex = INDEX_NONE;
        return false;
    }

    const float ToleranceSquared = FMath::Square(FMath::Max(0.f, PixelWorldTolerance));
    const float MaxProjectedDistance = MaxRayDistance > 0.f ? MaxRayDistance : TNumericLimits<float>::Max();
    float BestDistanceSquared = ToleranceSquared > 0.f ? ToleranceSquared : TNumericLimits<float>::Max();
    int32 BestIndex = INDEX_NONE;

    for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
    {
        const FVector RayToPoint = Points[PointIndex] - RayOrigin;
        const float ProjectedDistance = FVector::DotProduct(RayToPoint, SafeRayDirection);
        if (ProjectedDistance < 0.f || ProjectedDistance > MaxProjectedDistance)
        {
            continue;
        }

        const FVector ClosestPointOnRay = RayOrigin + SafeRayDirection * ProjectedDistance;
        const float DistanceSquared = FVector::DistSquared(Points[PointIndex], ClosestPointOnRay);
        if (DistanceSquared <= BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestIndex = PointIndex;
        }
    }

    HoverIndex = BestIndex;
    SelectedIndex = BestIndex;
    return BestIndex != INDEX_NONE;
}

bool FVehiclePhATNativeConvexTool::GetSelectedPointTransform(FTransform& OutTransform)
{
    using namespace VehiclePhATNativeConvexToolState;
    if (!Points.IsValidIndex(SelectedIndex))
    {
        return false;
    }

    OutTransform = FTransform(Points[SelectedIndex]);
    return true;
}

bool FVehiclePhATNativeConvexTool::MoveSelectedPoint(const FVector& NewPosition, bool bSnapToMesh, float MaxSnapDistance)
{
    using namespace VehiclePhATNativeConvexToolState;
    if (!Points.IsValidIndex(SelectedIndex))
    {
        return false;
    }

    FVector TargetPosition = NewPosition;
    if (bSnapToMesh)
    {
        SnapPointToNearestPreviewMeshVertex(NewPosition, MaxSnapDistance, TargetPosition);
    }

    Points[SelectedIndex] = TargetPosition;
    HoverIndex = SelectedIndex;
    return true;
}

bool FVehiclePhATNativeConvexTool::ApplySelectedPointDelta(const FVector& Delta, bool bSnapToMesh, float MaxSnapDistance)
{
    using namespace VehiclePhATNativeConvexToolState;
    if (!Points.IsValidIndex(SelectedIndex))
    {
        return false;
    }

    return MoveSelectedPoint(Points[SelectedIndex] + Delta, bSnapToMesh, MaxSnapDistance);
}

bool FVehiclePhATNativeConvexTool::RebuildViewportVertexMarkers(float MarkerRadius, FString& OutMessage)
{
    using namespace VehiclePhATNativeConvexToolState;
    if (!IsActive())
    {
        OutMessage = TEXT("Native convex tool is inactive.");
        return false;
    }

    USkeletalBodySetup* BodySetup = GetBodySetup();
    UPhysicsAsset* ActivePhysicsAsset = PhysicsAsset.Get();
    if (!ActivePhysicsAsset || !BodySetup)
    {
        OutMessage = TEXT("Cannot build convex vertex markers without a valid PhysicsAsset and body.");
        return false;
    }

    FScopedTransaction Transaction(NSLOCTEXT("VehiclePhATTools", "RebuildConvexVertexMarkers", "Rebuild Vehicle Convex Vertex Markers"));
    ActivePhysicsAsset->Modify();
    BodySetup->Modify();

    const float SafeRadius = FMath::Max(0.25f, MarkerRadius);
    const float HiddenRadius = 0.01f;

    if (MarkerStartIndex == INDEX_NONE || !BodySetup->AggGeom.SphereElems.IsValidIndex(MarkerStartIndex))
    {
        MarkerStartIndex = BodySetup->AggGeom.SphereElems.Num();
        MarkerAllocatedCount = 0;
    }

    const int32 AvailableMarkerSlots = FMath::Max(0, BodySetup->AggGeom.SphereElems.Num() - MarkerStartIndex);
    MarkerAllocatedCount = FMath::Min(MarkerAllocatedCount, AvailableMarkerSlots);

    for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
    {
        FKSphereElem* Marker = nullptr;
        if (PointIndex < MarkerAllocatedCount)
        {
            Marker = &BodySetup->AggGeom.SphereElems[MarkerStartIndex + PointIndex];
        }
        else
        {
            Marker = &BodySetup->AggGeom.SphereElems.AddDefaulted_GetRef();
            ++MarkerAllocatedCount;
        }

        Marker->Center = Points[PointIndex];
        Marker->Radius = PointIndex == SelectedIndex ? SafeRadius * 1.75f : SafeRadius;
    }

    for (int32 MarkerOffset = Points.Num(); MarkerOffset < MarkerAllocatedCount; ++MarkerOffset)
    {
        FKSphereElem& Marker = BodySetup->AggGeom.SphereElems[MarkerStartIndex + MarkerOffset];
        Marker.Radius = HiddenRadius;
    }

    MarkerCount = Points.Num();
    LastLiveUpdatePoints = Points;
    LastObservedMarkerPoints = Points;
    LastObservedMarkerChangeTime = FPlatformTime::Seconds();
    bPendingDebouncedUpdate = false;
    FVehiclePhATBodyUtils::MarkAssetChanged(ActivePhysicsAsset);
    OutMessage = FString::Printf(TEXT("Created %d native PhAT viewport vertex marker sphere(s). Move these markers with the standard PhAT transform gizmo, then Apply Convex."), MarkerCount);
    return true;
}

bool FVehiclePhATNativeConvexTool::AddViewportVertexMarker(FString& OutMessage)
{
    using namespace VehiclePhATNativeConvexToolState;
    if (!IsActive())
    {
        OutMessage = TEXT("Native convex tool is inactive.");
        return false;
    }

    FString PullMessage;
    PullPointsFromViewportVertexMarkers(PullMessage);

    FVector NewPoint = FVector::ZeroVector;
    if (Points.Num() > 0)
    {
        FBox Bounds(ForceInit);
        for (const FVector& ExistingPoint : Points)
        {
            Bounds += ExistingPoint;
        }

        const FVector Center = Bounds.GetCenter();
        const FVector Extent = Bounds.GetExtent();
        NewPoint = FVector(Bounds.Max.X + FMath::Max(10.f, Extent.X * 0.35f), Center.Y, Center.Z);
    }

    SelectedIndex = Points.Add(NewPoint);
    HoverIndex = SelectedIndex;

    FString MarkerMessage;
    if (!RebuildViewportVertexMarkers(2.5f, MarkerMessage))
    {
        OutMessage = MarkerMessage;
        return false;
    }

    LastLiveUpdatePoints.Reset();
    FString UpdateMessage;
    LiveUpdateConvexFromViewportVertexMarkers(UpdateMessage);
    OutMessage = FString::Printf(TEXT("Added marker vertex %d at %.3f %.3f %.3f. It is the active/larger marker; move it with the PhAT transform gizmo."), SelectedIndex, NewPoint.X, NewPoint.Y, NewPoint.Z);
    return true;
}

bool FVehiclePhATNativeConvexTool::PullPointsFromViewportVertexMarkers(FString& OutMessage)
{
    using namespace VehiclePhATNativeConvexToolState;
    const USkeletalBodySetup* BodySetup = GetBodySetup();
    if (!BodySetup || MarkerStartIndex == INDEX_NONE || MarkerCount <= 0 || !BodySetup->AggGeom.SphereElems.IsValidIndex(MarkerStartIndex))
    {
        OutMessage = TEXT("No active native PhAT viewport vertex markers were found.");
        return false;
    }

    const int32 SafeMarkerCount = FMath::Min(MarkerCount, BodySetup->AggGeom.SphereElems.Num() - MarkerStartIndex);
    Points.Reset();
    Points.Reserve(SafeMarkerCount);
    for (int32 MarkerOffset = 0; MarkerOffset < SafeMarkerCount; ++MarkerOffset)
    {
        Points.Add(BodySetup->AggGeom.SphereElems[MarkerStartIndex + MarkerOffset].Center);
    }

    HoverIndex = INDEX_NONE;
    SelectedIndex = Points.Num() > 0 ? 0 : INDEX_NONE;
    OutMessage = FString::Printf(TEXT("Pulled %d convex point(s) from native PhAT viewport marker spheres."), Points.Num());
    return Points.Num() > 0;
}

bool FVehiclePhATNativeConvexTool::RemoveViewportVertexMarkers(FString& OutMessage)
{
    using namespace VehiclePhATNativeConvexToolState;
    USkeletalBodySetup* BodySetup = GetBodySetup();
    UPhysicsAsset* ActivePhysicsAsset = PhysicsAsset.Get();
    if (!ActivePhysicsAsset || !BodySetup || MarkerStartIndex == INDEX_NONE || MarkerAllocatedCount <= 0)
    {
        OutMessage = TEXT("No native PhAT viewport vertex markers to clear.");
        return false;
    }

    if (!BodySetup->AggGeom.SphereElems.IsValidIndex(MarkerStartIndex))
    {
        MarkerStartIndex = INDEX_NONE;
        MarkerCount = 0;
        MarkerAllocatedCount = 0;
        OutMessage = TEXT("Native PhAT viewport vertex marker indices were already invalid.");
        return false;
    }

    FScopedTransaction Transaction(NSLOCTEXT("VehiclePhATTools", "ClearConvexVertexMarkers", "Clear Vehicle Convex Vertex Markers"));
    ActivePhysicsAsset->Modify();
    BodySetup->Modify();

    const int32 SafeMarkerCount = FMath::Min(MarkerAllocatedCount, BodySetup->AggGeom.SphereElems.Num() - MarkerStartIndex);
    for (int32 MarkerOffset = 0; MarkerOffset < SafeMarkerCount; ++MarkerOffset)
    {
        BodySetup->AggGeom.SphereElems[MarkerStartIndex + MarkerOffset].Radius = 0.01f;
    }

    MarkerCount = 0;
    FVehiclePhATBodyUtils::MarkAssetChanged(ActivePhysicsAsset);
    OutMessage = FString::Printf(TEXT("Cleared %d native PhAT viewport vertex marker sphere(s) without deleting selected PhAT primitives."), SafeMarkerCount);
    return true;
}

bool FVehiclePhATNativeConvexTool::LiveUpdateConvexFromViewportVertexMarkers(FString& OutMessage)
{
    using namespace VehiclePhATNativeConvexToolState;
    if (!IsActive())
    {
        OutMessage = TEXT("Native convex tool is inactive.");
        return false;
    }

    FString PullMessage;
    if (!PullPointsFromViewportVertexMarkers(PullMessage))
    {
        OutMessage = PullMessage;
        return false;
    }

    if (Points.Num() < 4)
    {
        OutMessage = TEXT("Live update needs at least four marker points.");
        return false;
    }

    const bool bPointsChanged = !ArePointArraysNearlyEqual(Points, LastLiveUpdatePoints, 0.01f);

    if (!bPointsChanged)
    {
        OutMessage = TEXT("Live update skipped; marker points have not changed.");
        return false;
    }

    USkeletalBodySetup* BodySetup = GetBodySetup();
    UPhysicsAsset* ActivePhysicsAsset = PhysicsAsset.Get();
    if (!ActivePhysicsAsset || !BodySetup)
    {
        OutMessage = TEXT("Live update has no valid PhysicsAsset/body.");
        return false;
    }

    if (BodySetup->AggGeom.ConvexElems.IsValidIndex(ConvexIndex))
    {
        FKConvexElem& Convex = BodySetup->AggGeom.ConvexElems[ConvexIndex];
        Convex.VertexData = Points;
        Convex.UpdateElemBox();
    }
    else
    {
        FKConvexElem& NewConvex = BodySetup->AggGeom.ConvexElems.AddDefaulted_GetRef();
        NewConvex.VertexData = Points;
        NewConvex.UpdateElemBox();
        ConvexIndex = BodySetup->AggGeom.ConvexElems.Num() - 1;
        Mode = EMode::Edit;
        bLiveCreatedConvex = true;
    }

    LastLiveUpdatePoints = Points;
    FVehiclePhATBodyUtils::MarkBodySetupGeometryChanged(ActivePhysicsAsset, BodySetup);
    OutMessage = FString::Printf(TEXT("Live-updated convex %d from %d viewport marker point(s)."), ConvexIndex, Points.Num());
    return true;
}

bool FVehiclePhATNativeConvexTool::DebouncedUpdateConvexFromViewportVertexMarkers(float QuietDelaySeconds, FString& OutMessage)
{
    using namespace VehiclePhATNativeConvexToolState;
    if (!IsActive())
    {
        OutMessage = TEXT("Native convex tool is inactive.");
        return false;
    }

    FString PullMessage;
    if (!PullPointsFromViewportVertexMarkers(PullMessage))
    {
        OutMessage = PullMessage;
        return false;
    }

    const double Now = FPlatformTime::Seconds();
    if (!ArePointArraysNearlyEqual(Points, LastObservedMarkerPoints, 0.01f))
    {
        LastObservedMarkerPoints = Points;
        LastObservedMarkerChangeTime = Now;
        bPendingDebouncedUpdate = true;
        OutMessage = TEXT("Marker movement detected; waiting for transform gizmo release/settle before updating convex.");
        return false;
    }

    if (!bPendingDebouncedUpdate)
    {
        OutMessage = TEXT("No pending marker movement to apply.");
        return false;
    }

    const double QuietDelay = FMath::Max(0.0, static_cast<double>(QuietDelaySeconds));
    if (Now - LastObservedMarkerChangeTime < QuietDelay)
    {
        OutMessage = TEXT("Marker movement is still settling.");
        return false;
    }

    const bool bUpdated = LiveUpdateConvexFromViewportVertexMarkers(OutMessage);
    if (bUpdated)
    {
        bPendingDebouncedUpdate = false;
        LastObservedMarkerPoints = Points;
    }
    return bUpdated;
}

void FVehiclePhATNativeConvexTool::AddPoint(const FVector& Point)
{
    using namespace VehiclePhATNativeConvexToolState;
    SelectedIndex = Points.Add(Point);
    HoverIndex = SelectedIndex;
}

void FVehiclePhATNativeConvexTool::AddPointSnappedToMesh(const FVector& Point, float MaxSnapDistance)
{
    using namespace VehiclePhATNativeConvexToolState;
    FVector SnappedPoint = Point;
    SnapPointToNearestPreviewMeshVertex(Point, MaxSnapDistance, SnappedPoint);
    SelectedIndex = Points.Add(SnappedPoint);
    HoverIndex = SelectedIndex;
}

bool FVehiclePhATNativeConvexTool::MoveHoveredPoint(const FVector& Point)
{
    using namespace VehiclePhATNativeConvexToolState;
    if (!Points.IsValidIndex(HoverIndex))
    {
        return false;
    }

    Points[HoverIndex] = Point;
    SelectedIndex = HoverIndex;
    return true;
}

bool FVehiclePhATNativeConvexTool::MoveHoveredPointSnappedToMesh(const FVector& Point, float MaxSnapDistance)
{
    FVector SnappedPoint = Point;
    SnapPointToNearestPreviewMeshVertex(Point, MaxSnapDistance, SnappedPoint);
    return MoveHoveredPoint(SnappedPoint);
}

bool FVehiclePhATNativeConvexTool::DeleteHoveredPoint()
{
    using namespace VehiclePhATNativeConvexToolState;
    if (!Points.IsValidIndex(HoverIndex))
    {
        return false;
    }

    Points.RemoveAt(HoverIndex);
    if (SelectedIndex == HoverIndex)
    {
        SelectedIndex = INDEX_NONE;
    }
    else if (SelectedIndex > HoverIndex)
    {
        --SelectedIndex;
    }
    HoverIndex = INDEX_NONE;
    return true;
}

bool FVehiclePhATNativeConvexTool::SnapPointToNearestPreviewMeshVertex(const FVector& Point, float MaxSnapDistance, FVector& OutSnappedPoint)
{
    const UPhysicsAsset* ActivePhysicsAsset = VehiclePhATNativeConvexToolState::PhysicsAsset.Get();
    if (!ActivePhysicsAsset || !ActivePhysicsAsset->PreviewSkeletalMesh.Get())
    {
        return false;
    }

    const USkeletalMesh* SkeletalMesh = ActivePhysicsAsset->PreviewSkeletalMesh.Get();
    const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
    if (!RenderData || RenderData->LODRenderData.Num() == 0)
    {
        return false;
    }

    const FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[0];
    const FPositionVertexBuffer& PositionVertexBuffer = LODData.StaticVertexBuffers.PositionVertexBuffer;
    if (PositionVertexBuffer.GetNumVertices() == 0)
    {
        return false;
    }

    const float MaxDistanceSquared = MaxSnapDistance > 0.f ? FMath::Square(MaxSnapDistance) : TNumericLimits<float>::Max();
    float BestDistanceSquared = MaxDistanceSquared;
    bool bFound = false;
    FVector BestPoint = Point;

    for (uint32 VertexIndex = 0; VertexIndex < PositionVertexBuffer.GetNumVertices(); ++VertexIndex)
    {
        const FVector VertexPosition(PositionVertexBuffer.VertexPosition(VertexIndex));
        const float DistanceSquared = FVector::DistSquared(Point, VertexPosition);
        if (DistanceSquared <= BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestPoint = VertexPosition;
            bFound = true;
        }
    }

    if (!bFound)
    {
        return false;
    }

    OutSnappedPoint = BestPoint;
    return true;
}

bool FVehiclePhATNativeConvexTool::FindNearestPreviewMeshVertexToRay(const FVector& RayOrigin, const FVector& RayDirection, float MaxRayDistance, FVector& OutSnappedPoint, int32& OutVertexIndex)
{
    const UPhysicsAsset* ActivePhysicsAsset = VehiclePhATNativeConvexToolState::PhysicsAsset.Get();
    if (!ActivePhysicsAsset || !ActivePhysicsAsset->PreviewSkeletalMesh.Get())
    {
        return false;
    }

    const USkeletalMesh* SkeletalMesh = ActivePhysicsAsset->PreviewSkeletalMesh.Get();
    const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
    if (!RenderData || RenderData->LODRenderData.Num() == 0)
    {
        return false;
    }

    const FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[0];
    const FPositionVertexBuffer& PositionVertexBuffer = LODData.StaticVertexBuffers.PositionVertexBuffer;
    if (PositionVertexBuffer.GetNumVertices() == 0)
    {
        return false;
    }

    const FVector SafeRayDirection = RayDirection.GetSafeNormal();
    if (SafeRayDirection.IsNearlyZero())
    {
        return false;
    }

    const float MaxDistanceSquared = MaxRayDistance > 0.f ? FMath::Square(MaxRayDistance) : TNumericLimits<float>::Max();
    float BestDistanceSquared = MaxDistanceSquared;
    bool bFound = false;
    FVector BestPoint = FVector::ZeroVector;
    int32 BestIndex = INDEX_NONE;

    for (uint32 VertexIndex = 0; VertexIndex < PositionVertexBuffer.GetNumVertices(); ++VertexIndex)
    {
        const FVector VertexPosition(PositionVertexBuffer.VertexPosition(VertexIndex));
        const FVector RayToVertex = VertexPosition - RayOrigin;
        const float ProjectedDistance = FVector::DotProduct(RayToVertex, SafeRayDirection);
        if (ProjectedDistance < 0.f)
        {
            continue;
        }

        const FVector ClosestPointOnRay = RayOrigin + SafeRayDirection * ProjectedDistance;
        const float DistanceSquared = FVector::DistSquared(VertexPosition, ClosestPointOnRay);
        if (DistanceSquared <= BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestPoint = VertexPosition;
            BestIndex = static_cast<int32>(VertexIndex);
            bFound = true;
        }
    }

    if (!bFound)
    {
        return false;
    }

    OutSnappedPoint = BestPoint;
    OutVertexIndex = BestIndex;
    return true;
}

bool FVehiclePhATNativeConvexTool::UpdateHoverFromRay(const FVector& RayOrigin, const FVector& RayDirection, float MaxRayDistance)
{
    FVector SnappedPoint;
    int32 MeshVertexIndex = INDEX_NONE;
    if (!FindNearestPreviewMeshVertexToRay(RayOrigin, RayDirection, MaxRayDistance, SnappedPoint, MeshVertexIndex))
    {
        VehiclePhATNativeConvexToolState::HoverIndex = INDEX_NONE;
        return false;
    }

    float BestPointDistanceSquared = TNumericLimits<float>::Max();
    int32 BestPointIndex = INDEX_NONE;
    const TArray<FVector>& Points = VehiclePhATNativeConvexToolState::Points;
    for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
    {
        const float DistanceSquared = FVector::DistSquared(Points[PointIndex], SnappedPoint);
        if (DistanceSquared < BestPointDistanceSquared)
        {
            BestPointDistanceSquared = DistanceSquared;
            BestPointIndex = PointIndex;
        }
    }

    VehiclePhATNativeConvexToolState::HoverIndex = BestPointIndex;
    return BestPointIndex != INDEX_NONE;
}

bool FVehiclePhATNativeConvexTool::AddPointFromRay(const FVector& RayOrigin, const FVector& RayDirection, float MaxRayDistance)
{
    FVector SnappedPoint;
    int32 MeshVertexIndex = INDEX_NONE;
    if (!FindNearestPreviewMeshVertexToRay(RayOrigin, RayDirection, MaxRayDistance, SnappedPoint, MeshVertexIndex))
    {
        return false;
    }

    VehiclePhATNativeConvexToolState::Points.Add(SnappedPoint);
    VehiclePhATNativeConvexToolState::HoverIndex = VehiclePhATNativeConvexToolState::Points.Num() - 1;
    VehiclePhATNativeConvexToolState::SelectedIndex = VehiclePhATNativeConvexToolState::HoverIndex;
    return true;
}

bool FVehiclePhATNativeConvexTool::MoveHoveredPointFromRay(const FVector& RayOrigin, const FVector& RayDirection, float MaxRayDistance)
{
    FVector SnappedPoint;
    int32 MeshVertexIndex = INDEX_NONE;
    if (!FindNearestPreviewMeshVertexToRay(RayOrigin, RayDirection, MaxRayDistance, SnappedPoint, MeshVertexIndex))
    {
        return false;
    }

    return MoveHoveredPoint(SnappedPoint);
}

bool FVehiclePhATNativeConvexTool::HandleViewportRayAction(EViewportAction Action, const FVector& RayOrigin, const FVector& RayDirection, float MaxRayDistance, FString& OutMessage)
{
    if (!IsActive())
    {
        OutMessage = TEXT("Native convex tool is inactive.");
        return false;
    }

    switch (Action)
    {
    case EViewportAction::Hover:
    {
        const bool bUpdatedHover = UpdateHoverFromRay(RayOrigin, RayDirection, MaxRayDistance);
        OutMessage = bUpdatedHover
            ? FString::Printf(TEXT("Hovered convex point %d."), GetHoverIndex())
            : TEXT("No convex point under cursor.");
        return bUpdatedHover;
    }
    case EViewportAction::PrimaryPress:
    {
        UpdateHoverFromRay(RayOrigin, RayDirection, MaxRayDistance);
        if (GetPoints().IsValidIndex(GetHoverIndex()))
        {
            const bool bSelected = SelectHoveredPoint();
            OutMessage = bSelected
                ? FString::Printf(TEXT("Selected convex point %d for the PhAT transform gizmo."), GetSelectedPointIndex())
                : TEXT("Could not select hovered convex point.");
            return bSelected;
        }

        const bool bAdded = AddPointFromRay(RayOrigin, RayDirection, MaxRayDistance);
        OutMessage = bAdded
            ? FString::Printf(TEXT("Added convex point %d."), GetHoverIndex())
            : TEXT("Could not add convex point from viewport ray.");
        return bAdded;
    }
    case EViewportAction::PrimaryDrag:
    {
        const bool bMoved = MoveHoveredPointFromRay(RayOrigin, RayDirection, MaxRayDistance);
        OutMessage = bMoved
            ? FString::Printf(TEXT("Dragged convex point %d."), GetHoverIndex())
            : TEXT("No hovered convex point to drag.");
        return bMoved;
    }
    case EViewportAction::SecondaryPress:
    {
        UpdateHoverFromRay(RayOrigin, RayDirection, MaxRayDistance);
        const int32 DeletedIndex = GetHoverIndex();
        const bool bDeleted = DeleteHoveredPoint();
        OutMessage = bDeleted
            ? FString::Printf(TEXT("Deleted convex point %d."), DeletedIndex)
            : TEXT("No hovered convex point to delete.");
        return bDeleted;
    }
    default:
        break;
    }

    OutMessage = TEXT("Unsupported native convex viewport action.");
    return false;
}

void FVehiclePhATNativeConvexTool::BuildViewportRenderData(TArray<FViewportPoint>& OutPoints, TArray<FViewportSegment>& OutSegments)
{
    using namespace VehiclePhATNativeConvexToolState;
    OutPoints.Reset();
    OutSegments.Reset();

    if (Mode == EMode::Inactive)
    {
        return;
    }

    for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
    {
        const bool bHovered = PointIndex == HoverIndex;
        const bool bSelected = PointIndex == SelectedIndex;
        FViewportPoint& ViewportPoint = OutPoints.AddDefaulted_GetRef();
        ViewportPoint.Position = Points[PointIndex];
        ViewportPoint.bHovered = bHovered || bSelected;
        ViewportPoint.Color = bHovered ? FLinearColor::Yellow : (bSelected ? FLinearColor(1.f, 0.55f, 0.f, 1.f) : FLinearColor(0.1f, 0.65f, 1.f, 1.f));
        ViewportPoint.Size = (bHovered || bSelected) ? 14.f : 9.f;
    }

    if (Points.Num() < 2)
    {
        return;
    }

    for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
    {
        const int32 NextPointIndex = (PointIndex + 1) % Points.Num();
        if (PointIndex == Points.Num() - 1 && Points.Num() < 3)
        {
            break;
        }

        FViewportSegment& Segment = OutSegments.AddDefaulted_GetRef();
        Segment.Start = Points[PointIndex];
        Segment.End = Points[NextPointIndex];
        Segment.Color = FLinearColor(0.1f, 0.65f, 1.f, 1.f);
    }
}

bool FVehiclePhATNativeConvexTool::Apply(FString& OutMessage)
{
    using namespace VehiclePhATNativeConvexToolState;
    USkeletalBodySetup* BodySetup = GetBodySetup();
    if (!BodySetup)
    {
        OutMessage = TEXT("Native convex tool has no valid body setup.");
        return false;
    }

    if (Mode == EMode::Create)
    {
        const bool bApplied = FVehiclePhATConvexUtils::AddConvexFromPoints(PhysicsAsset.Get(), BodySetup, Points, OutMessage);
        bLiveCreatedConvex = false;
        return bApplied;
    }

    if (Mode == EMode::Edit)
    {
        const bool bApplied = FVehiclePhATConvexUtils::ReplaceConvexFromPoints(PhysicsAsset.Get(), BodySetup, ConvexIndex, Points, OutMessage);
        bLiveCreatedConvex = false;
        return bApplied;
    }

    OutMessage = TEXT("Native convex tool is inactive.");
    return false;
}

USkeletalBodySetup* FVehiclePhATNativeConvexTool::GetBodySetup()
{
    return FVehiclePhATBodyUtils::FindBodySetup(VehiclePhATNativeConvexToolState::PhysicsAsset.Get(), VehiclePhATNativeConvexToolState::BodyBone);
}

void FVehiclePhATNativeConvexTool::LoadExistingConvex()
{
    using namespace VehiclePhATNativeConvexToolState;
    const USkeletalBodySetup* BodySetup = GetBodySetup();
    if (!BodySetup || !BodySetup->AggGeom.ConvexElems.IsValidIndex(ConvexIndex))
    {
        return;
    }

    Points = BodySetup->AggGeom.ConvexElems[ConvexIndex].VertexData;
}
