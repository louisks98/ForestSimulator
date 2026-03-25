#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Structs.h"
#include "TreeStructureDataAsset.generated.h"

UCLASS(BlueprintType)
class FORESTSIMULATOR_API UTreeStructureDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	TArray<FTreeNode> Nodes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	TArray<FBranchEdge> Edges;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	TArray<FBranchModule> Modules;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	TArray<FLeafInstance> Leaves;
};
