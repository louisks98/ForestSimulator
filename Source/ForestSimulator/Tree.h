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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tree")
	UTreeStructureDataAsset* TreeStructure = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tree")
	int MinNumSides = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tree")
	int MaxNumSides = 12;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tree")
	float MeshNoiseStrength = 0.02f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tree")
	FWoodProperties WoodProperties;
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
	
	FTreeNode* GetNode(int32 Index);
	
	int EdgeCount() const { return BranchEdges.Num();};
	FBranchEdge* GetEdge(int Index);
	FBranchEdge* GetParentEdge(int32 Index);
	TArray<FBranchEdge*> GetChildEdges(int32 Index);

protected:
	virtual void BeginPlay() override;

private:
	
	UPROPERTY()
	UProceduralMeshComponent* MeshComponent;
	UPROPERTY()
	UInstancedStaticMeshComponent* LeavesInstanceComponent;

	
	TArray<FTreeNode> TreeNodes;
	TArray<FBranchModule> BranchModules;
	TArray<FBranchEdge> BranchEdges;
	TArray<FLeafInstance> LeafInstances;
	
	TMap<int32, int32> EdgeEndingAt;
	TMap<int32, TArray<int32>> EdgesStartingAt;
	
	TArray<FQuat> LeafRotations;
	
	void DebugDrawTree();
	void BuildTreeMesh();
	void BuildBranchMesh(int Index, TArray<int>& Chain);
	Vertices ComputeVertices(int Resolution, int NodeIndex, FVector Right, FVector Forward, float UV_v);
	TArray<int> ComputeIndices(int Resolution, int Offset);
};
