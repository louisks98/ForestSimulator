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
	TreeStructure->Nodes.Add(RootNode);
	for (int i = 1; i <= TreeData->TrunkSegments; i++)
	{
		FTreeNode TrunkNode = FTreeNode();
		TrunkNode.Position = FVector(RootNode.Position.X, RootNode.Position.Y, RootNode.Position.Z + (TreeData->TrunkHeight / TreeData->TrunkSegments));
		TrunkNode.Orientation = FRotationMatrix::MakeFromX(FVector::UpVector).ToQuat();
		TrunkNode.ParentIndex = TreeStructure->Nodes.Num() - 1;
		FBranchEdge BranchEdge = FBranchEdge();
		BranchEdge.NodeStart = TreeStructure->Nodes.Num() - 1;
		BranchEdge.NodeEnd = TreeStructure->Nodes.Num();
		TreeStructure->Nodes.Add(TrunkNode);
		TreeStructure->Edges.Add(BranchEdge);

		RootNode = TrunkNode;
	}
	

	FBud TrunkBud{RootNode};
	TrunkBud.BudIndex = TreeStructure->Nodes.Num() - 1;
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
	RelocateNodes(TreeStructure);
	ComputeRadii(TreeData, TreeStructure);

	for (int i = 0; i < TreeData->Subdivisions; i++)
		Subdivide(TreeStructure);
	
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
			FVector Tropism = TreeData->TropismWeight * FVector::UpVector;
			FVector BiasedDirection = (AveragePoint + Tropism).GetSafeNormal();
			FVector ChildPosition = ParentPosition + BranchLength * BiasedDirection;
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
			QValues.FindOrAdd(Bud.BudIndex) += 1.0f;
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
		float RadiusSumSquared = pow(TreeStructure->Nodes[Children[0]].Radius, TreeData->BranchRadiusExponent);
		for (int j = 1; j < Children.Num(); j++)
			RadiusSumSquared += pow(TreeStructure->Nodes[Children[j]].Radius, TreeData->BranchRadiusExponent);
			
		TreeStructure->Nodes[i].Radius = pow(RadiusSumSquared, 1.0f/TreeData->BranchRadiusExponent);
	}

	FTreeNode RootNode = TreeStructure->Nodes[0];
	float MaxHeight = TreeData->CrownSize + TreeData->TrunkHeight;
	for (int i = 1; i < TreeStructure->Nodes.Num(); i++)
	{
		FTreeNode& Node = TreeStructure->Nodes[i];
		const float Distance = FVector::Distance(RootNode.Position, Node.Position);
		const float Factor = FMath::Lerp(1.5f, 1.0f, Distance / MaxHeight);
		Node.Radius *= Factor;
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

			NewNodes[NewEdge.NodeEnd].ParentIndex = NewEdge.NodeStart;
			BranchEdges.Add(NewEdge);
		}
	}
	
	TreeStructure->Nodes = NewNodes;
	TreeStructure->Edges = BranchEdges;
}

void UTreeGenerator::Subdivide(UTreeStructureDataAsset* TreeStructure)
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
		
		int Parent = TreeStructure->Nodes[i].ParentIndex;
		if (Parent == INDEX_NONE)
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

	TArray<FBranchEdge> NewBranches;
	for (const auto& Edge : TreeStructure->Edges)
	{
		int Start = Edge.NodeStart;
		int End = Edge.NodeEnd;

		if (!ToKeep.Contains(Start))
			continue;
		
		TArray<int> Chain;
		Chain.Add(Start);
		while (!ToKeep.Contains(End) && ChildrenOf.Contains(End) && ChildrenOf[End].Num() == 1)
		{
			Chain.Add(End);
			End = ChildrenOf[End][0];
		}
		Chain.Add(End);

		if (Chain.Num() < 2)
		{
			if (ToKeep.Contains(End) && ToKeep.Contains(Start))
			{
				FBranchEdge NewEdge;
				NewEdge.NodeStart = RemappedIndices[Start];
				NewEdge.NodeEnd = RemappedIndices[End];
				NewBranches.Add(NewEdge);
			}
			continue;
		}
		
		TArray<FTreeNode> ChaikinPoints;
		for (int i = 0; i < Chain.Num() - 1; i++)
		{
			FTreeNode Node = TreeStructure->Nodes[Chain[i]];
			FTreeNode ChildNode = TreeStructure->Nodes[Chain[i + 1]];
			FVector Q = 0.75f * Node.Position + 0.25f * ChildNode.Position;
			FVector R = 0.25f * Node.Position + 0.75f * ChildNode.Position;

			FTreeNode QNode;
			QNode.Position = Q;
			QNode.Orientation = Node.Orientation;
			QNode.Radius = FMath::Lerp(Node.Radius, ChildNode.Radius, 0.25f);
			FTreeNode RNode;
			RNode.Position = R;
			RNode.Orientation = Node.Orientation;
			RNode.Radius = FMath::Lerp(Node.Radius, ChildNode.Radius, 0.75f);

			ChaikinPoints.Add(QNode);
			ChaikinPoints.Add(RNode);
		}
		
		int ParentIndex = RemappedIndices[Start];
		for (int i = 0; i < ChaikinPoints.Num(); i += 2)
		{
			FBranchEdge ParentToQ;
			FBranchEdge QToR;
			FTreeNode QNode = ChaikinPoints[i];
			QNode.ParentIndex = ParentIndex;
			FTreeNode RNode = ChaikinPoints[i + 1];

			ParentToQ.NodeStart = ParentIndex;
			ParentToQ.NodeEnd = NewNodes.Num();
			RNode.ParentIndex = NewNodes.Num();
			NewNodes.Add(QNode);
			
			QToR.NodeStart = NewNodes.Num() - 1;
			QToR.NodeEnd = NewNodes.Num();
			NewNodes.Add(RNode);

			NewBranches.Add(ParentToQ);
			NewBranches.Add(QToR);
			ParentIndex = NewNodes.Num() - 1;
		}

		FBranchEdge FinalEdge;
		FinalEdge.NodeStart = ParentIndex;
		FinalEdge.NodeEnd = RemappedIndices[End];
		NewBranches.Add(FinalEdge);
	}

	for (const auto& Edge : NewBranches)
		NewNodes[Edge.NodeEnd].ParentIndex = Edge.NodeStart;

	TreeStructure->Nodes = NewNodes;
	TreeStructure->Edges = NewBranches;
}

void UTreeGenerator::RelocateNodes(UTreeStructureDataAsset* TreeStructure)
{
	TArray<FVector> OriginalPositions;
	for (const auto& Node : TreeStructure->Nodes)
		OriginalPositions.Add(Node.Position);

	for (int i = 1; i < TreeStructure->Nodes.Num(); i++)
	{
		int Parent = TreeStructure->Nodes[i].ParentIndex;
		TreeStructure->Nodes[i].Position = OriginalPositions[Parent] + 0.5 * (OriginalPositions[i] - OriginalPositions[Parent]);
	}
}


