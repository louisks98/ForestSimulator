#include "TreeGenerator.h"

#include "TreeStructureDataAsset.h"
#include "TreePropertiesDataAsset.h"

void UTreeGenerator::GenerateTreeStructure(UTreePropertiesDataAsset* TreePropertiesData, UTreeStructureDataAsset* TreeStructureData)
{
	TreeStructureData->Edges.Empty();
	TreeStructureData->Nodes.Empty();
	TreeStructureData->Leaves.Empty();
	TreeStructureData->Modules.Empty();
	
	FRandomStream Random{TreePropertiesData->RandomSeed};
	
	GenerateStructure(Random, TreePropertiesData, TreeStructureData);
}

void UTreeGenerator::GenerateStructure(FRandomStream& RandomStream, UTreePropertiesDataAsset* TreeData, UTreeStructureDataAsset* TreeStructure)
{
	TArray<FVector> AttractionPoints = TArray<FVector>();
	GenerateAttractionPoints(RandomStream, TreeData, AttractionPoints);

	FTreeNode RootNode = FTreeNode();
	FTreeNode TrunkNode = FTreeNode();
	TrunkNode.Position = FVector(RootNode.Position.X, RootNode.Position.Y, RootNode.Position.Z + TreeData->TreeHeight / 2.0f);
	FBranchEdge BranchEdge = FBranchEdge();
	BranchEdge.NodeStart = 0;
	BranchEdge.NodeEnd = 1;
	TreeStructure->Nodes.Add(RootNode);
	TreeStructure->Nodes.Add(TrunkNode);
	TreeStructure->Edges.Add(BranchEdge);
	
	GenerateNodes(TreeData, TreeStructure, AttractionPoints);
}

void UTreeGenerator::GenerateNodes(UTreePropertiesDataAsset* TreeData, UTreeStructureDataAsset* TreeStructure,
	TArray<FVector>& AttractionPoints)
{
	TMap<int, TArray<FVector>> AttractionPointsPerNode = TMap<int, TArray<FVector>>();
	for (const auto AttractionPoint : AttractionPoints)
	{
		int ClosestIndex = 0;
		for (int i = 0; i < TreeStructure->Nodes.Num(); i++)
		{
			FTreeNode CurrentClosestNode = TreeStructure->Nodes[ClosestIndex];
			FTreeNode Node = TreeStructure->Nodes[i];
			const float LengthToClosest = (AttractionPoint - CurrentClosestNode.Position).Length();
			const float Length = (AttractionPoint - Node.Position).Length();
			if (Length > TreeData->AttractionRadius)
				continue;

			if (Length < LengthToClosest)
				ClosestIndex = i;
		}

		float FinalDist = (AttractionPoint - TreeStructure->Nodes[ClosestIndex].Position).Length();
		if (FinalDist > TreeData->AttractionRadius)
			continue;
			
		AttractionPointsPerNode.FindOrAdd(ClosestIndex).Add(AttractionPoint);
	}

	
	for (auto PointsPerNode : AttractionPointsPerNode)
	{
		if (PointsPerNode.Value.Num() == 0)
			continue;
		
		FTreeNode Node = TreeStructure->Nodes[PointsPerNode.Key];
		TArray<FVector> Points = PointsPerNode.Value;

		FVector AveragePoint = FVector::ZeroVector;
		for (const auto Point : Points)
		{
			AveragePoint += (Point - Node.Position).GetSafeNormal();
		}
		AveragePoint.Normalize();
		
		FVector ChildPosition = Node.Position + TreeData->BranchLength * AveragePoint;
		FTreeNode ChildNode;
		ChildNode.Position = ChildPosition;
		TreeStructure->Nodes.Add(ChildNode);
		FBranchEdge BranchEdge;
		BranchEdge.NodeStart = PointsPerNode.Key;
		BranchEdge.NodeEnd = TreeStructure->Nodes.Num() - 1;
		TreeStructure->Edges.Add(BranchEdge);
	}

	TArray<FVector> PointsToRemove = TArray<FVector>();
	for (int i = 0; i < TreeStructure->Nodes.Num(); i++)
	{
		FTreeNode Node = TreeStructure->Nodes[i];
		for (const auto AttractionPoint : AttractionPoints)
		{
			const float Length = (AttractionPoint - Node.Position).Length();
			if (Length > TreeData->KillRadius)
				continue;
			
			PointsToRemove.Add(AttractionPoint);
		}
	}

	for (auto PointToRemove : PointsToRemove)
		AttractionPoints.Remove(PointToRemove);
	
	if (AttractionPoints.Num() == 0)
		return;

	GenerateNodes(TreeData, TreeStructure, AttractionPoints);
}

void UTreeGenerator::GenerateAttractionPoints(FRandomStream& RandomStream, UTreePropertiesDataAsset* TreeData,
	TArray<FVector>& AttractionPoints)
{
	for (int i = 0; i < TreeData->NbAttractionPoints; i++)
	{
		const float u = RandomStream.FRand();
		const float v = RandomStream.FRand();
		
		const float Phi = 2 * PI * v;
		float Theta = FMath::Acos(1.0f - 2.0f * u);
		if (TreeData->TreeShape == ETreeShape::HalfSphere)
			Theta = FMath::Acos(1.0f - u);
		
		if (TreeData->PointDistribution == EPointDistribution::Surface)
		{
			const float x = TreeData->CrownSize * cos(Phi) * sin(Theta);
			const float y = TreeData->CrownSize * sin(Phi) * sin(Theta);
			const float z = TreeData->CrownSize * cos(Theta);
			FVector Point{x, y , z + TreeData->TreeHeight};
			AttractionPoints.Add(Point);
		}
		else if (TreeData->PointDistribution == EPointDistribution::Uniform)
		{
			const float w = RandomStream.FRand();
			const float r = TreeData->CrownSize * FMath::Pow(w, 1.0f / 3.0f);
			const float x = r * cos(Phi) * sin(Theta);
			const float y = r * sin(Phi) * sin(Theta);
			const float z = r * cos(Theta);
			FVector Point{x, y , z + TreeData->TreeHeight};
			AttractionPoints.Add(Point);
		}
	}
}




