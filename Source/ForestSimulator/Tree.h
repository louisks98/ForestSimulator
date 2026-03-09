#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Tree.generated.h"

class UTreeStructureDataAsset;

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
	UPROPERTY()
	float Stiffness = 1.f;
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
	float Thickness = 0.f;
	UPROPERTY()
	float CharThickness = 0.f;
	UPROPERTY()
	float Moisture = 0.5f;
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
