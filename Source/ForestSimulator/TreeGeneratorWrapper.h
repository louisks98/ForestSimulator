#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TreeGeneratorWrapper.generated.h"

class UTreePropertiesDataAsset;
class UTreeStructureDataAsset;

UCLASS()
class FORESTSIMULATOR_API UTreeGeneratorWrapper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable)
	static void GenerateTreeStructure(UTreePropertiesDataAsset* TreePropertiesData, UTreeStructureDataAsset* TreeStructureData);
};
