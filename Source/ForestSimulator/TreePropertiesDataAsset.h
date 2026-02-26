#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TreePropertiesDataAsset.generated.h"

UCLASS(BlueprintType)
class FORESTSIMULATOR_API UTreePropertiesDataAsset: public UDataAsset
{

	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties", meta=(ToolTip="Starting radius at the base"))
	float TrunkRadius;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties", meta=(ToolTip="Radius at which a tip is declared and a leaf is placed"))
	float MinRadius;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties", meta=(ToolTip="Length = Radius × this factor"))
	float SegmentLengthFactor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties", meta=(ToolTip="Number of children at each fork"))
	int32 BranchCount;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties", meta=(ToolTip="Pitch angle for side branches away from parent heading"))
	float BranchAngle;	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties", meta=(ToolTip="Roll offset between successive siblings (137.5° is the golden angle)"))
	float PhyllotaxisAngle = 137.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties", meta=(ToolTip="How strongly gravity bends each segment"))
	float TropismStrength;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties", meta=(ToolTip="Small pitch/roll on the dominant axis to produce trunk lean and sway"))
	float ApicalCurvature;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties", meta=(ToolTip="Equal fork vs dominant axis"))
	bool Sympodial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties", meta=(ToolTip="For computing initial edge mass"))
	float WoodDensity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Properties")
	int32 RandomSeed;	
};
