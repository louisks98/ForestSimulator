#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Structs.h"
#include "Tree.generated.h"

class UTreeStructureDataAsset;

UCLASS()
class FORESTSIMULATOR_API ATree : public AActor
{
	GENERATED_BODY()
	
public:

	TArray<FTreeNode> TreeNodes;
	TArray<FBranchModule> BranchModules;
	TArray<FBranchEdge> BranchEdges;
	TArray<FLeafInstance> LeafInstances;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tree")
	UTreeStructureDataAsset* TreeData = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tree")
	int MinNumSides = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tree")
	int MaxNumSides = 12;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tree")
	UStaticMesh* LeafMesh; 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool Debug = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool RenderLeaves = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool RenderBranches = true;
	

	ATree();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	
	UPROPERTY()
	UProceduralMeshComponent* MeshComponent;
	UPROPERTY()
	UInstancedStaticMeshComponent* LeavesInstanceComponent;

	TArray<FQuat> LeafRotations;
	
	void DebugDrawTree();
	void BuildTreeMesh();
	void BuildBranchMesh(int Index, FTreeNode Start, FTreeNode End) const;
};
