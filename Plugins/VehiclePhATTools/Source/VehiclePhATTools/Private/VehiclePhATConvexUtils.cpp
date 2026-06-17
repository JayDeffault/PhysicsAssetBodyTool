#include "VehiclePhATConvexUtils.h"
#include "VehiclePhATBodyUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "ScopedTransaction.h"

bool FVehiclePhATConvexUtils::AddConvexFromPoints(UPhysicsAsset* P, USkeletalBodySetup* B, const TArray<FVector>& Points, FString& Msg)
{
    if (!P || !B || Points.Num() < 4)
    {
        Msg = TEXT("Need a selected body and at least four points.");
        return false;
    }

    FScopedTransaction Tx(NSLOCTEXT("VehiclePhATTools", "AddConvex", "Add Vehicle Convex Body"));
    P->Modify();
    B->Modify();

    FKConvexElem E;
    E.VertexData = Points;
    E.UpdateElemBox();
    B->AggGeom.ConvexElems.Add(E);

    FVehiclePhATBodyUtils::MarkBodySetupGeometryChanged(P, B);
    Msg = TEXT("Convex element added from point cloud and PhysicsAsset editor views were refreshed.");
    return true;
}

bool FVehiclePhATConvexUtils::ReplaceConvexFromPoints(UPhysicsAsset* P, USkeletalBodySetup* B, int32 I, const TArray<FVector>& Points, FString& Msg)
{
    if (!P || !B || !B->AggGeom.ConvexElems.IsValidIndex(I) || Points.Num() < 4)
    {
        Msg = TEXT("Invalid convex index or point count.");
        return false;
    }

    FScopedTransaction Tx(NSLOCTEXT("VehiclePhATTools", "ReplaceConvex", "Replace Vehicle Convex Body"));
    P->Modify();
    B->Modify();

    B->AggGeom.ConvexElems[I].VertexData = Points;
    B->AggGeom.ConvexElems[I].UpdateElemBox();

    FVehiclePhATBodyUtils::MarkBodySetupGeometryChanged(P, B);
    Msg = TEXT("Convex element rebuilt from edited point cloud and PhysicsAsset editor views were refreshed.");
    return true;
}
