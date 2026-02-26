#include "Tree.h"
#include "TreeStructureDataAsset.h"

ATree::ATree()
{
	PrimaryActorTick.bCanEverTick = true;

}

void ATree::BeginPlay()
{
	Super::BeginPlay();

	if (TreeData)
	{
		TreeNodes      = TreeData->Nodes;
		BranchEdges    = TreeData->Edges;
		BranchModules  = TreeData->Modules;
		LeafInstances  = TreeData->Leaves;
		
		UE_LOG(LogTemp, Log,
		       TEXT("ATree '%s': loaded %d nodes, %d edges, %d modules, %d leaves."),
		       *GetName(),
		       TreeNodes.Num(), BranchEdges.Num(),
		       BranchModules.Num(), LeafInstances.Num());

		DebugDrawTree();
	}
	else
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("ATree '%s': no TreeData asset assigned."), *GetName());
	}
}

void ATree::DebugDrawTree()
{
	TArray<int32> Indices = TArray<int32>();
	FTransform WorldPosition = GetActorTransform();
	for (int i = 0; i < BranchEdges.Num(); i++)
	{
		FTreeNode Start = TreeNodes[BranchEdges[i].NodeStart];
		FTreeNode End = TreeNodes[BranchEdges[i].NodeEnd];

		FVector StartPosition = WorldPosition.TransformPosition(Start.Position);
		FVector EndPosition = WorldPosition.TransformPosition(End.Position);
		if (!Indices.Contains(BranchEdges[i].NodeStart))
		{
			DrawDebugSphere(GetWorld(), StartPosition, Start.Radius, 12, FColor::Blue, true, 1.0f);
			Indices.Add(BranchEdges[i].NodeStart);
		}

		if (!Indices.Contains(BranchEdges[i].NodeEnd))
		{
			DrawDebugSphere(GetWorld(), EndPosition, End.Radius, 12, FColor::Blue, true, 1.0f);
			Indices.Add(BranchEdges[i].NodeEnd);
		}
		
		DrawDebugLine(GetWorld(), StartPosition, EndPosition, FColor::Blue, true, 1.0f);
	}
}

void ATree::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

