#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

	ATree();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	void DebugDrawTree();
};
