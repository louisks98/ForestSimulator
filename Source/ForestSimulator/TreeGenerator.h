#pragma once

#include "CoreMinimal.h"
#include "TreeStructureDataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TreeGenerator.generated.h"

class USkeletalMesh;
class UTreeStructureDataAsset;

struct FTurtleFrame
{
	FVector Position;
	FQuat Frame;
};

UCLASS()
class FORESTSIMULATOR_API UTreeGenerator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable)
	static void GenerateTreeStructure(UTreePropertiesDataAsset* TreePropertiesData, UTreeStructureDataAsset* TreeStructureData);

private:
	static void GenerateStructure(FRandomStream& RandomStream, UTreePropertiesDataAsset* TreeData, UTreeStructureDataAsset* TreeStructure, FTurtleFrame TurtleFrame, float BranchRadius, int ModuleIndex, int PreviousNodeIndex);
};
