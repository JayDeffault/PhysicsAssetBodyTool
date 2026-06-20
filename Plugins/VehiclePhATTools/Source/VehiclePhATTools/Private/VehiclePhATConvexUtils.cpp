#include "VehiclePhATConvexUtils.h"
#include "VehiclePhATBodyUtils.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool ParsePointArray(const TArray<TSharedPtr<FJsonValue>>& Values, TArray<FVector>& OutPoints)
{
    OutPoints.Reset();
    for (const TSharedPtr<FJsonValue>& Value : Values)
    {
        const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
        if (!Value.IsValid() || !Value->TryGetArray(Array) || !Array || Array->Num() < 3)
        {
            continue;
        }

        OutPoints.Add(FVector(
            static_cast<float>((*Array)[0]->AsNumber()),
            static_cast<float>((*Array)[1]->AsNumber()),
            static_cast<float>((*Array)[2]->AsNumber())));
    }
    return OutPoints.Num() >= 4;
}

void ParseJsonUcXEntry(const TSharedPtr<FJsonObject>& Object, TArray<TArray<FVector>>& OutHulls)
{
    if (!Object.IsValid())
    {
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* Vertices = nullptr;
    if ((Object->TryGetArrayField(TEXT("vertices"), Vertices) || Object->TryGetArrayField(TEXT("Vertices"), Vertices) || Object->TryGetArrayField(TEXT("points"), Vertices)) && Vertices)
    {
        TArray<FVector> Points;
        if (ParsePointArray(*Vertices, Points))
        {
            OutHulls.Add(MoveTemp(Points));
        }
    }
}

bool ParseJsonUcX(const FString& Text, TArray<TArray<FVector>>& OutHulls, FString& OutMessage)
{
    TSharedPtr<FJsonValue> RootValue;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, RootValue) || !RootValue.IsValid())
    {
        OutMessage = TEXT("JSON parse failed.");
        return false;
    }

    if (RootValue->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& RootArray = RootValue->AsArray();
        TArray<FVector> DirectPoints;
        if (ParsePointArray(RootArray, DirectPoints))
        {
            OutHulls.Add(MoveTemp(DirectPoints));
        }
        else
        {
            for (const TSharedPtr<FJsonValue>& Entry : RootArray)
            {
                if (Entry.IsValid() && Entry->Type == EJson::Object)
                {
                    ParseJsonUcXEntry(Entry->AsObject(), OutHulls);
                }
            }
        }
    }
    else if (RootValue->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> RootObject = RootValue->AsObject();
        const TArray<TSharedPtr<FJsonValue>>* HullArray = nullptr;
        if (RootObject->TryGetArrayField(TEXT("ucx"), HullArray) || RootObject->TryGetArrayField(TEXT("UCX"), HullArray) || RootObject->TryGetArrayField(TEXT("convex"), HullArray) || RootObject->TryGetArrayField(TEXT("hulls"), HullArray))
        {
            for (const TSharedPtr<FJsonValue>& Entry : *HullArray)
            {
                if (Entry.IsValid() && Entry->Type == EJson::Object)
                {
                    ParseJsonUcXEntry(Entry->AsObject(), OutHulls);
                }
                else if (Entry.IsValid() && Entry->Type == EJson::Array)
                {
                    TArray<FVector> Points;
                    if (ParsePointArray(Entry->AsArray(), Points))
                    {
                        OutHulls.Add(MoveTemp(Points));
                    }
                }
            }
        }
        else
        {
            ParseJsonUcXEntry(RootObject, OutHulls);
        }
    }

    OutMessage = FString::Printf(TEXT("Parsed %d UCX hull(s) from JSON."), OutHulls.Num());
    return OutHulls.Num() > 0;
}

bool ParseAsciiFbxUcX(const FString& Text, TArray<TArray<FVector>>& OutHulls, FString& OutMessage)
{
    int32 SearchOffset = 0;
    while (true)
    {
        const int32 UcxIndex = Text.Find(TEXT("UCX_"), ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchOffset);
        if (UcxIndex == INDEX_NONE)
        {
            break;
        }

        const int32 VerticesIndex = Text.Find(TEXT("Vertices:"), ESearchCase::IgnoreCase, ESearchDir::FromStart, UcxIndex);
        if (VerticesIndex == INDEX_NONE)
        {
            SearchOffset = UcxIndex + 4;
            continue;
        }

        const int32 ArrayStart = Text.Find(TEXT("a:"), ESearchCase::CaseSensitive, ESearchDir::FromStart, VerticesIndex);
        if (ArrayStart == INDEX_NONE)
        {
            SearchOffset = VerticesIndex + 9;
            continue;
        }

        const int32 ArrayEnd = Text.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ArrayStart);
        if (ArrayEnd == INDEX_NONE)
        {
            break;
        }

        FString NumberText = Text.Mid(ArrayStart + 2, ArrayEnd - (ArrayStart + 2));
        NumberText.ReplaceInline(TEXT(","), TEXT(" "));
        NumberText.ReplaceInline(TEXT("\n"), TEXT(" "));
        NumberText.ReplaceInline(TEXT("\r"), TEXT(" "));
        NumberText.ReplaceInline(TEXT("\t"), TEXT(" "));

        TArray<FString> Tokens;
        NumberText.ParseIntoArrayWS(Tokens);
        TArray<FVector> Points;
        for (int32 TokenIndex = 0; TokenIndex + 2 < Tokens.Num(); TokenIndex += 3)
        {
            Points.Add(FVector(FCString::Atof(*Tokens[TokenIndex]), FCString::Atof(*Tokens[TokenIndex + 1]), FCString::Atof(*Tokens[TokenIndex + 2])));
        }
        if (Points.Num() >= 4)
        {
            OutHulls.Add(MoveTemp(Points));
        }

        SearchOffset = ArrayEnd + 1;
    }

    OutMessage = FString::Printf(TEXT("Parsed %d UCX hull(s) from ASCII FBX."), OutHulls.Num());
    return OutHulls.Num() > 0;
}
}

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

bool FVehiclePhATConvexUtils::ImportUcXFromFile(UPhysicsAsset* PhysicsAsset, USkeletalBodySetup* BodySetup, const FString& FilePath, FString& OutMessage)
{
    if (!PhysicsAsset || !BodySetup)
    {
        OutMessage = TEXT("Need a selected PhysicsAsset and target body before importing UCX collision.");
        return false;
    }

    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *FilePath))
    {
        OutMessage = FString::Printf(TEXT("Could not read UCX file: %s"), *FilePath);
        return false;
    }

    TArray<TArray<FVector>> Hulls;
    FString ParseMessage;
    const FString Extension = FPaths::GetExtension(FilePath).ToLower();
    const bool bParsed = Extension == TEXT("json")
        ? ParseJsonUcX(Text, Hulls, ParseMessage)
        : ParseAsciiFbxUcX(Text, Hulls, ParseMessage);
    if (!bParsed || Hulls.Num() == 0)
    {
        OutMessage = ParseMessage + TEXT(" Supported JSON: {\"ucx\":[{\"vertices\":[[X,Y,Z], ...]}]} or raw [[X,Y,Z], ...]. Supported FBX: ASCII FBX with UCX_* mesh Vertices arrays.");
        return false;
    }

    FScopedTransaction Tx(NSLOCTEXT("VehiclePhATTools", "ImportUCXCollision", "Import UCX Collision"));
    PhysicsAsset->Modify();
    BodySetup->Modify();

    int32 AddedCount = 0;
    for (const TArray<FVector>& Points : Hulls)
    {
        if (Points.Num() < 4)
        {
            continue;
        }

        FKConvexElem& Convex = BodySetup->AggGeom.ConvexElems.AddDefaulted_GetRef();
        Convex.VertexData = Points;
        Convex.UpdateElemBox();
        ++AddedCount;
    }

    if (AddedCount <= 0)
    {
        OutMessage = TEXT("UCX file was parsed, but no hull had at least four vertices.");
        return false;
    }

    FVehiclePhATBodyUtils::MarkBodySetupGeometryChanged(PhysicsAsset, BodySetup);
    OutMessage = FString::Printf(TEXT("Imported %d UCX convex hull(s) from %s."), AddedCount, *FilePath);
    return true;
}
