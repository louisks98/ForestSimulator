#include "TreeGenerator.h"
#include "Tree.h"
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
	double StartTime = FPlatformTime::Seconds();
	TArray<FVector> AttractionPoints = TArray<FVector>();
	GenerateAttractionPoints(RandomStream, TreeData, AttractionPoints);

	FTreeNode RootNode = FTreeNode();
	FTreeNode TrunkNode = FTreeNode();
	TrunkNode.Position = FVector(RootNode.Position.X, RootNode.Position.Y, RootNode.Position.Z + TreeData->TrunkHeight);
	TrunkNode.Orientation = FRotationMatrix::MakeFromX(FVector::UpVector).ToQuat();
	TrunkNode.ParentIndex = 0;
	FBranchEdge BranchEdge = FBranchEdge();
	BranchEdge.NodeStart = 0;
	BranchEdge.NodeEnd = 1;
	TreeStructure->Nodes.Add(RootNode);
	TreeStructure->Nodes.Add(TrunkNode);
	TreeStructure->Edges.Add(BranchEdge);

	FBud TrunkBud{TrunkNode};
	TrunkBud.BudIndex = 1;
	TArray<FBud> Buds = TArray<FBud>();
	Buds.Add(TrunkBud);

	// for (auto Point : AttractionPoints)
	// {
	// 	FTreeNode AttractionNode = FTreeNode();
	// 	AttractionNode.Position = Point;
	// 	TreeStructure->Nodes.Add(AttractionNode);
	// }
	
	GenerateNodes(0, TreeData, TreeStructure, AttractionPoints, Buds);
	DecimateNodes(TreeStructure);
	ComputeRadii(TreeData, TreeStructure);
	double EndTime = FPlatformTime::Seconds();
	double ElapsedTime = EndTime - StartTime;
	UE_LOG(LogTemp, Warning, TEXT("Tree graph generation execution time: %f seconds. Generated %d nodes and %d edges"), ElapsedTime, TreeStructure->Nodes.Num(), TreeStructure->Edges.Num());
}

void UTreeGenerator::GenerateNodes(int Iterations, UTreePropertiesDataAsset* TreeData, UTreeStructureDataAsset* TreeStructure,
	TArray<FVector>& AttractionPoints , TArray<FBud>& Buds)
{
	if (Iterations >= TreeData->MaxIteration)
		return;
	
	TArray<FVector> PointsToRemove = TArray<FVector>();
	TMap<int, TArray<FVector>> AttractionPointsPerBud = TMap<int, TArray<FVector>>();
	bool AnyPointFound = false;

	for (const auto& Point : AttractionPoints)
	{
		int ClosestIndex = -1;
		for (int i = 0; i < Buds.Num(); i++)
		{
			FBud Bud = Buds[i];
			float ConeRadius = TreeData->PerceptionLength * tan(  FMath::DegreesToRadians(TreeData->PerceptionAngle / 2.0f));
			float ConeDist = FVector::DotProduct(Point - Bud.Position, Bud.Orientation.GetForwardVector());
			if (ConeDist < 0 || ConeDist > TreeData->PerceptionLength)
				continue;

			float RadiusAtPoint = (ConeDist / TreeData->PerceptionLength) * ConeRadius;
			FVector Ortho = (Point - Bud.Position) - ConeDist * Bud.Orientation.GetForwardVector();
			if (Ortho.Length() > RadiusAtPoint)
				continue;

			const float Length = (Point - Bud.Position).Length();
			if (ClosestIndex == -1 || Length < (Point - Buds[ClosestIndex].Position).Length())
				ClosestIndex = i;
		}

		if (ClosestIndex != -1)
		{
			AttractionPointsPerBud.FindOrAdd(ClosestIndex).Add(Point);
			AnyPointFound = true;
		}
	}

	if (!AnyPointFound)
		return;

	TMap<int, TPair<float, float>> QSplit;
	float Q = DoBasipetalPass(TreeStructure, Buds, AttractionPointsPerBud, QSplit);
	DoAcropetalPass(Q, TreeData, TreeStructure, Buds, QSplit);
	
	TArray<FBud> NewBuds = TArray<FBud>();
	for (const auto& PointsPerBud : AttractionPointsPerBud)
	{
		if (PointsPerBud.Value.Num() == 0)
			continue;
		
		FBud Bud = Buds[PointsPerBud.Key];
		TArray<FVector> Points = PointsPerBud.Value;

		FVector AveragePoint = FVector::ZeroVector;
		for (const auto& Point : Points)
		{
			AveragePoint += (Point - Bud.Position).GetSafeNormal();
		}
		AveragePoint.Normalize();
		FVector GrowthDirection = AveragePoint;
		
		int NbNewSegments = FMath::Floor(Bud.Vigor);
		if (NbNewSegments == 0)
			continue;
		
		float BranchLength = Bud.Vigor / NbNewSegments;

		FVector ParentPosition = Bud.Position;
		int ParentIndex = Bud.BudIndex;
		FTreeNode ChildNode;
		for (int i = 0 ; i < NbNewSegments ; i++)
		{
			FVector ChildPosition = ParentPosition + BranchLength * AveragePoint;
			//GrowthDirection = (GrowthDirection + NbNewSegments * FVector::DownVector).GetSafeNormal();
			ChildNode.Position = ChildPosition;
			ChildNode.Orientation = FRotationMatrix::MakeFromX(GrowthDirection).ToQuat();
			ChildNode.ParentIndex = ParentIndex;
			TreeStructure->Nodes.Add(ChildNode);
			int ChildIndex = TreeStructure->Nodes.Num() - 1;
		
			FBranchEdge BranchEdge;
			BranchEdge.NodeStart = ParentIndex;
			BranchEdge.NodeEnd = ChildIndex;
			TreeStructure->Edges.Add(BranchEdge);
			
			ParentPosition = ChildPosition;
			ParentIndex = ChildIndex;
			
			FQuat LateralQuat = FQuat(GrowthDirection, FMath::DegreesToRadians(137.5 * (i + 1)));
			FQuat RotationQuat = FQuat(FVector::CrossProduct(GrowthDirection, FVector::UpVector).GetSafeNormal(), FMath::DegreesToRadians(TreeData->BranchAngle));
			FVector RotatedGrowthDir =  RotationQuat.RotateVector(GrowthDirection);
			FVector LateralGrowthDirection = LateralQuat.RotateVector(RotatedGrowthDir);
			
			FBud LateralBud{ChildNode.Position, FRotationMatrix::MakeFromX(LateralGrowthDirection).ToQuat()};
			LateralBud.BudIndex = ChildIndex;
			NewBuds.Add(LateralBud);
		}
		
		FBud ApicalBud{ChildNode};
		ApicalBud.BudIndex = TreeStructure->Nodes.Num() - 1;
		NewBuds.Add(ApicalBud);
	}

	TArray<FBud> AllBuds = TArray(Buds);
	AllBuds.Append(NewBuds);
	for (int i = 0; i < AllBuds.Num(); i++)
	{
		FBud Bud = AllBuds[i];
		for (const auto& Point : AttractionPoints)
		{
			if (FVector::DistSquared(Point, Bud.Position) < FMath::Square(TreeData->OccupancyRadius))
				PointsToRemove.Add(Point);
		}
	}
	
	for (auto PointToRemove : PointsToRemove)
		AttractionPoints.Remove(PointToRemove);
	
	if (AttractionPoints.Num() == 0)
		return;
	
	GenerateNodes(Iterations + 1, TreeData, TreeStructure, AttractionPoints, NewBuds);
}

void UTreeGenerator::GenerateAttractionPoints(FRandomStream& RandomStream, UTreePropertiesDataAsset* TreeData,
	TArray<FVector>& AttractionPoints)
{
	for (int i = 0; i < TreeData->NbAttractionPoints; i++)
	{
		const float TrunkHeight = TreeData->TrunkHeight;
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
			FVector Point{x, y , z + TrunkHeight};
			AttractionPoints.Add(Point);
		}
		else if (TreeData->PointDistribution == EPointDistribution::Uniform)
		{
			const float w = RandomStream.FRand();
			const float r = TreeData->CrownSize * FMath::Pow(w, 1.0f / 3.0f);
			const float x = r * cos(Phi) * sin(Theta);
			const float y = r * sin(Phi) * sin(Theta);
			const float z = r * cos(Theta);
			FVector Point{x, y , z + TrunkHeight};
			AttractionPoints.Add(Point);
		}
	}
}

float UTreeGenerator::DoBasipetalPass(UTreeStructureDataAsset* TreeStructure, TArray<FBud>& Buds,
	TMap<int, TArray<FVector>>& AttractionPointsPerBud, TMap<int, TPair<float, float>>& OutQSplit)
{
	TMap<int, TArray<int>> ChildrenOf;
	for (const auto& Edge : TreeStructure->Edges)
		ChildrenOf.FindOrAdd(Edge.NodeStart).Add(Edge.NodeEnd);

	TMap<int, float> QValues;
	for (int i = 0; i < Buds.Num(); i++)
	{
		FBud Bud = Buds[i];
		if (AttractionPointsPerBud.Contains(i))
			QValues.FindOrAdd(Bud.BudIndex) += AttractionPointsPerBud.Num() > 0 ? 1.0f : 0.0f;
		else
			QValues.FindOrAdd(Bud.BudIndex) = 0.0f;
	}

	TMap<int, TPair<float, float>> QSplit;
	for (int i = TreeStructure->Nodes.Num() - 1; i >= 0; i--)
	{
		if (!ChildrenOf.Contains(i))
			continue;

		const TArray<int>& Children = ChildrenOf[i];
		if (Children.Num() == 1)
			QValues.FindOrAdd(i) += QValues.FindOrAdd(Children[0]);
		else
		{
			float Qm = QValues.FindOrAdd(Children[0]);
			float Ql = 0.0f;
			for (int j = 1; j < Children.Num(); j++)
				Ql += QValues.FindOrAdd(Children[j]);

			QSplit.Add(i, TPair<float, float>(Qm, Ql));
			QValues.FindOrAdd(i) += Qm + Ql;
		}
	}

	OutQSplit = QSplit;
	return QValues[0];
}

void UTreeGenerator::DoAcropetalPass(float QBase, UTreePropertiesDataAsset* TreeData, UTreeStructureDataAsset* TreeStructure, TArray<FBud>& Buds, TMap<int, TPair<float, float>>& QSplit)
{
	float VBase = QBase * TreeData->ResourceCoefficient;

	TMap<int, TArray<int>> ChildrenOf;
	for (const auto& Edge : TreeStructure->Edges)
		ChildrenOf.FindOrAdd(Edge.NodeStart).Add(Edge.NodeEnd);

	TMap<int, float> Vigor;
	Vigor.Add(0, VBase);
	for (int i = 0; i < TreeStructure->Nodes.Num(); i++)
	{
		if (!ChildrenOf.Contains(i))
			continue;
		
		const TArray<int>& Children = ChildrenOf[i];
		float v = Vigor.FindOrAdd(i);
		if (Children.Num() == 1)
			Vigor.Add(Children[0], v);
		else
		{
			float Qm = QSplit[i].Key;
			float Ql = QSplit[i].Value;
			float Vm;
			if (Qm == 0.0f && Ql == 0.0f)
				Vm = v * 0.5;
			else
				Vm = v * (TreeData->ApicalControl * Qm) / (TreeData->ApicalControl * Qm + (1 - TreeData->ApicalControl) * Ql);

			float Vl = v - Vm;
			Vigor.Add(Children[0], Vm);
			for (int j = 1; j < Children.Num(); j++)
				Vigor.Add(Children[j], Vl / (Children.Num() - 1));
		}
	}

	for (auto& Bud : Buds)
		Bud.Vigor = Vigor.FindOrAdd(Bud.BudIndex);
}

void UTreeGenerator::ComputeRadii(UTreePropertiesDataAsset* TreeData, UTreeStructureDataAsset* TreeStructure)
{
	TMap<int, TArray<int>> ChildrenOf;
	for (const auto& Edge : TreeStructure->Edges)
		ChildrenOf.FindOrAdd(Edge.NodeStart).Add(Edge.NodeEnd);
	
	for (auto& Node : TreeStructure->Nodes)
		Node.Radius = 0.25f; // initial radius for terminal nodes
	
	for (int i = TreeStructure->Nodes.Num() - 1; i >= 0; i--)
	{
		if (!ChildrenOf.Contains(i))
			continue;

		const TArray<int>& Children = ChildrenOf[i];
		if (Children.Num() == 1)
			TreeStructure->Nodes[i].Radius = TreeStructure->Nodes[Children[0]].Radius;
		else
		{
			float RadiusSumSquared = pow(TreeStructure->Nodes[Children[0]].Radius, TreeData->BranchRadiusExponent);
			for (int j = 1; j < Children.Num(); j++)
				RadiusSumSquared += pow(TreeStructure->Nodes[Children[j]].Radius, TreeData->BranchRadiusExponent);
			
			TreeStructure->Nodes[i].Radius = pow(RadiusSumSquared, 1.0f/TreeData->BranchRadiusExponent);
		}
	}
}

void UTreeGenerator::DecimateNodes(UTreeStructureDataAsset* TreeStructure)
{
	TMap<int, TArray<int>> ChildrenOf;
	for (const auto& Edge : TreeStructure->Edges)
		ChildrenOf.FindOrAdd(Edge.NodeStart).Add(Edge.NodeEnd);

	TSet<int> ToKeep;
	for (int i = 0; i < TreeStructure->Nodes.Num(); i++)
	{
		if (!ChildrenOf.Contains(i) || ChildrenOf[i].Num() > 1)
		{
			ToKeep.Add(i);
			continue;
		}
		
		int Child = ChildrenOf[i][0];
		int Parent = TreeStructure->Nodes[i].ParentIndex;
		if (Parent == INDEX_NONE)
		{
			ToKeep.Add(i);
			continue;
		}

		FVector Direction = TreeStructure->Nodes[i].Position - TreeStructure->Nodes[Parent].Position;
		FVector ChildDirection = TreeStructure->Nodes[Child].Position - TreeStructure->Nodes[i].Position;
		float Dot = FVector::DotProduct(Direction.GetSafeNormal(), ChildDirection.GetSafeNormal());
		if (Dot < 0.99f)
			ToKeep.Add(i);
	}

	TMap<int, int> RemappedIndices;
	TArray<FTreeNode> NewNodes;
	for (int i = 0; i < TreeStructure->Nodes.Num(); i++)
	{
		if (ToKeep.Contains(i))
		{
			RemappedIndices.Add(i, NewNodes.Num());
			NewNodes.Add(TreeStructure->Nodes[i]);
		}
	}

	TArray<FBranchEdge> BranchEdges;
	for (const auto& Edge : TreeStructure->Edges)
	{
		int Start = Edge.NodeStart;
		int End = Edge.NodeEnd;

		while (!ToKeep.Contains(End) && ChildrenOf.Contains(End) && ChildrenOf[End].Num() == 1)
			End = ChildrenOf[End][0];

		if (ToKeep.Contains(Start) && ToKeep.Contains(End))
		{
			FBranchEdge NewEdge;
			NewEdge.NodeStart = RemappedIndices[Start];
			NewEdge.NodeEnd = RemappedIndices[End];
			if (BranchEdges.ContainsByPredicate([&]( const FBranchEdge& E ) { return E.NodeEnd == RemappedIndices[End] && E.NodeStart == RemappedIndices[Start]; }))
				continue;

			BranchEdges.Add(NewEdge);
		}
	}

	for (auto& Node : NewNodes)
	{
		if (Node.ParentIndex != INDEX_NONE && RemappedIndices.Contains(Node.ParentIndex))
			Node.ParentIndex = RemappedIndices[Node.ParentIndex];
	}

	TreeStructure->Nodes = NewNodes;
	TreeStructure->Edges = BranchEdges;
}




