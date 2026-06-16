#include "VehiclePhATNativeConvexTool.h"

#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
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
TArray<FVector> Points;
}

void FVehiclePhATNativeConvexTool::StartCreate(UPhysicsAsset* PhysicsAsset, FName BodyBone)
{
    using namespace VehiclePhATNativeConvexToolState;
    Mode = EMode::Create;
    VehiclePhATNativeConvexToolState::PhysicsAsset = PhysicsAsset;
    VehiclePhATNativeConvexToolState::BodyBone = BodyBone;
    ConvexIndex = INDEX_NONE;
    HoverIndex = INDEX_NONE;
    Points.Reset();
    UE_LOG(LogVehiclePhATTools, Log, TEXT("Native convex create mode started for body '%s'."), *BodyBone.ToString());
}

void FVehiclePhATNativeConvexTool::StartEdit(UPhysicsAsset* PhysicsAsset, FName BodyBone, int32 InConvexIndex)
{
    using namespace VehiclePhATNativeConvexToolState;
    Mode = EMode::Edit;
    VehiclePhATNativeConvexToolState::PhysicsAsset = PhysicsAsset;
    VehiclePhATNativeConvexToolState::BodyBone = BodyBone;
    ConvexIndex = InConvexIndex;
    HoverIndex = INDEX_NONE;
    Points.Reset();
    LoadExistingConvex();
    UE_LOG(LogVehiclePhATTools, Log, TEXT("Native convex edit mode started for body '%s' convex %d."), *BodyBone.ToString(), InConvexIndex);
}

void FVehiclePhATNativeConvexTool::Stop()
{
    using namespace VehiclePhATNativeConvexToolState;
    Mode = EMode::Inactive;
    PhysicsAsset.Reset();
    BodyBone = NAME_None;
    ConvexIndex = INDEX_NONE;
    HoverIndex = INDEX_NONE;
    Points.Reset();
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

void FVehiclePhATNativeConvexTool::SetHoverIndex(int32 Index)
{
    VehiclePhATNativeConvexToolState::HoverIndex = Index;
}

int32 FVehiclePhATNativeConvexTool::GetHoverIndex()
{
    return VehiclePhATNativeConvexToolState::HoverIndex;
}

void FVehiclePhATNativeConvexTool::AddPoint(const FVector& Point)
{
    VehiclePhATNativeConvexToolState::Points.Add(Point);
}

void FVehiclePhATNativeConvexTool::AddPointSnappedToMesh(const FVector& Point, float MaxSnapDistance)
{
    FVector SnappedPoint = Point;
    SnapPointToNearestPreviewMeshVertex(Point, MaxSnapDistance, SnappedPoint);
    VehiclePhATNativeConvexToolState::Points.Add(SnappedPoint);
}

bool FVehiclePhATNativeConvexTool::MoveHoveredPoint(const FVector& Point)
{
    using namespace VehiclePhATNativeConvexToolState;
    if (!Points.IsValidIndex(HoverIndex))
    {
        return false;
    }

    Points[HoverIndex] = Point;
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
    HoverIndex = INDEX_NONE;
    return true;
}

bool FVehiclePhATNativeConvexTool::SnapPointToNearestPreviewMeshVertex(const FVector& Point, float MaxSnapDistance, FVector& OutSnappedPoint)
{
    const UPhysicsAsset* ActivePhysicsAsset = VehiclePhATNativeConvexToolState::PhysicsAsset.Get();
    if (!ActivePhysicsAsset || !ActivePhysicsAsset->PreviewSkeletalMesh)
    {
        return false;
    }

    const USkeletalMesh* SkeletalMesh = ActivePhysicsAsset->PreviewSkeletalMesh;
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
        return FVehiclePhATConvexUtils::AddConvexFromPoints(PhysicsAsset.Get(), BodySetup, Points, OutMessage);
    }

    if (Mode == EMode::Edit)
    {
        return FVehiclePhATConvexUtils::ReplaceConvexFromPoints(PhysicsAsset.Get(), BodySetup, ConvexIndex, Points, OutMessage);
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
