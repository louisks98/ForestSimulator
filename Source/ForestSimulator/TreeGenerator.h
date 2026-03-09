#pragma once

#include "CoreMinimal.h"
#include "TreeStructureDataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TreeGenerator.generated.h"

class USkeletalMesh;
class UTreeStructureDataAsset;

UCLASS()
class FORESTSIMULATOR_API UTreeGenerator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable)
	static void GenerateTreeStructure(UTreePropertiesDataAsset* TreePropertiesData, UTreeStructureDataAsset* TreeStructureData);

private:
	static void GenerateStructure(FRandomStream& RandomStream, UTreePropertiesDataAsset* TreeData, UTreeStructureDataAsset* TreeStructure);
	static void GenerateNodes(int Iterations, UTreePropertiesDataAsset* TreeData, UTreeStructureDataAsset* TreeStructure, TArray<FVector>& AttractionPoints, TArray<FBud>& Buds);
	static void GenerateAttractionPoints(FRandomStream& RandomStream, UTreePropertiesDataAsset* TreeData, TArray<FVector>& AttractionPoints);

	static float DoBasipetalPass(UTreeStructureDataAsset* TreeStructure, TArray<FBud>& Buds, TMap<int, TArray<FVector>>& AttractionPointsPerBud, TMap<int, TPair<float, float>>& OutQSplit);
	static void DoAcropetalPass(int QBase, UTreePropertiesDataAsset* TreeData, UTreeStructureDataAsset* TreeStructure, TArray<FBud>& Buds, TMap<int, TPair<float, float>>& QSplit);
};
