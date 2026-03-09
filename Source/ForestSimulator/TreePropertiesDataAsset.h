#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TreePropertiesDataAsset.generated.h"

UENUM(BlueprintType)
enum class ETreeShape : uint8
{
	Sphere,
	HalfSphere,
	Cone,
	Cylinder,
	Cube
};

UENUM(BlueprintType)
enum class EPointDistribution : uint8
{
	Uniform,
	Surface,
};

UCLASS(BlueprintType)
class FORESTSIMULATOR_API UTreePropertiesDataAsset: public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	ETreeShape TreeShape;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	EPointDistribution PointDistribution;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	float CrownSize;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	float TreeHeight;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	float TrunkHeight;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	int NbAttractionPoints;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	float BranchLength;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	float BranchAngle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	float PerceptionAngle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	float PerceptionLength;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	float OccupancyRadius;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	float ResourceCoefficient = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	float ApicalControl = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	int32 MaxIteration;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	int32 RandomSeed;
};
