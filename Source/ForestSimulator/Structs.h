#pragma once
#include "CoreMinimal.h"
#include "Structs.generated.h"

USTRUCT(BlueprintType)
struct FTreeNode
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ParentIndex = INDEX_NONE;
	UPROPERTY()
	FVector Position = FVector::ZeroVector;
	UPROPERTY()
	FQuat Orientation = FQuat::Identity;
	UPROPERTY()
	float Radius = 1.f;

	bool operator==(const FTreeNode& Other) const
	{
		if (Position != Other.Position)
			return false;
		if (Orientation != Other.Orientation)
			return false;
		if (Radius != Other.Radius)
			return false;

		return true;
	}
};

USTRUCT(BlueprintType)
struct FBud
{
	GENERATED_BODY()
	FBud() = default;
	explicit FBud(const FTreeNode& Node) : Position(Node.Position), Orientation(Node.Orientation){}
	FBud(const FVector& P, const FQuat& O) : Position(P), Orientation(O){}

	UPROPERTY()
	int32 BudIndex = INDEX_NONE;
	UPROPERTY()
	FVector Position = FVector::ZeroVector;
	UPROPERTY()
	FQuat Orientation = FQuat::Identity;
	UPROPERTY()
	float Vigor = 0;
};

USTRUCT(BlueprintType)
struct FBranchEdge
{
	GENERATED_BODY()

	UPROPERTY()
	int32 NodeStart = INDEX_NONE;
	UPROPERTY()
	int32 NodeEnd = INDEX_NONE;
	UPROPERTY()
	float Length = 0.f;
	
	UPROPERTY()
	float Temperature = 0.f;
	UPROPERTY()
	float Mass = 0.f;
	UPROPERTY()
	float InitialThickness = 0.f;
	UPROPERTY()
	float Thickness = 0.f;
	UPROPERTY()
	float CharThickness = 0.f;
	UPROPERTY()
	float InitialArea = 0.f;
	UPROPERTY()
	float Area = 0.f;
	UPROPERTY()
	float CharInsulation = 0.f;
	UPROPERTY()
	float WaterContent = 0.5f;

	bool operator==(const FBranchEdge& Other) const
	{
		if (NodeStart != Other.NodeStart)
			return false;
		if (NodeEnd != Other.NodeEnd)
			return false;
		if (Length != Other.Length)
			return false;
		if (Temperature != Other.Temperature)
			return false;
		if (Mass != Other.Mass)
			return false;
		if (Thickness != Other.Thickness)
			return false;
		if (CharThickness != Other.CharThickness)
			return false;
		if (WaterContent != Other.WaterContent)
			return false;
		
		return true;
	}
};

USTRUCT(BlueprintType)
struct FBranchModule
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> EdgeIndices;
	UPROPERTY()
	int32 ParentModuleIndex = INDEX_NONE;
	UPROPERTY()
	float Temperature = 0.f;
	UPROPERTY()
	float Mass = 0.f;
};

USTRUCT(BlueprintType)
struct FLeafInstance
{
	GENERATED_BODY()

	UPROPERTY()
	int32 AttachedNodeIndex = INDEX_NONE;
	UPROPERTY()
	float Mass = 1.f;
	UPROPERTY()
	float Area = 1.f;
};

USTRUCT(BlueprintType)
struct FWoodProperties
{
	GENERATED_BODY()
	
	UPROPERTY()
	float Density = 800.0f;
	UPROPERTY()
	float ContractionFactor = 0.5f;
	UPROPERTY()
	float MinimumValueCharring = 0.1f;
	UPROPERTY()
	float RateCharInsulation = 50.0f;
	UPROPERTY() // value * 0.01f
	float MassLossRate = 0.25f * 0.01f;
	UPROPERTY()
	float DryWoodCoefficient = 0.05;
	UPROPERTY()
	float WetWoodCoefficient = DryWoodCoefficient * 0.1f;
	UPROPERTY()
	float AmountOfSmokeFromBurning = 16.0;
	UPROPERTY()
	float TemperatureDiffusion = 0.00000002f;
};

struct Vertices
{
	TArray<FVector> Positions = TArray<FVector>();
	TArray<FVector> Normals = TArray<FVector>();
	TArray<FVector2D> UVs = TArray<FVector2D>();
	TArray<FColor> Colors = TArray<FColor>();
	
	void Append(const Vertices& V)
	{
		Positions.Append(V.Positions);
		Normals.Append(V.Normals);
		UVs.Append(V.UVs);
		Colors.Append(V.Colors);
	}
};
