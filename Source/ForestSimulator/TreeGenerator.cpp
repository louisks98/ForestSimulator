#include "TreeGenerator.h"


TreeGenerator::TreeGenerator(UTreePropertiesDataAsset* TreePropertiesData, UTreeStructureDataAsset* TreeStructureData)
{
	TreeStructureData->Edges.Empty();
	TreeStructureData->Nodes.Empty();
	TreeStructureData->Leaves.Empty();
	TreeStructureData->Modules.Empty();
	
	RandomStream = {TreePropertiesData->RandomSeed};
	TreeProperties = TreePropertiesData;
	TreeStructure = TreeStructureData;
}

UTreeStructureDataAsset* TreeGenerator::GenerateTreeStructure()
{
	GenerateAttractionPoints();
	TArray<FBud> Buds = InitializeTrunk();
	GenerateNodes(0, Buds);
	DecimateNodes();
	RelocateNodesTowardsParent();
	ComputeRadii();
	
	for (int i = 0; i < TreeProperties->Subdivisions; i++)
		Subdivide();
	
	return TreeStructure;
}

void TreeGenerator::GenerateAttractionPoints()
{
	for (int i = 0; i < TreeProperties->NbAttractionPoints; i++)
	{
		const float TrunkHeight = TreeProperties->TrunkHeight - TreeProperties->TrunkHeight / TreeProperties->TrunkSegments;
		const float u = RandomStream.FRand();
		const float v = RandomStream.FRand();
		
		const float Phi = 2 * PI * v;
		float Theta = FMath::Acos(1.0f - 2.0f * u);
		if (TreeProperties->TreeShape == ETreeShape::HalfSphere)
			Theta = FMath::Acos(1.0f - u);
		
		if (TreeProperties->PointDistribution == EPointDistribution::Surface)
		{
			const float x = TreeProperties->CrownSize * cos(Phi) * sin(Theta);
			const float y = TreeProperties->CrownSize * sin(Phi) * sin(Theta);
			const float z = TreeProperties->CrownSize * cos(Theta);
			FVector Point{x, y , z + TrunkHeight};
			AttractionPoints.Add(Point);
		}
		else if (TreeProperties->PointDistribution == EPointDistribution::Uniform)
		{
			const float w = RandomStream.FRand();
			const float r = TreeProperties->CrownSize * FMath::Pow(w, 1.0f / 3.0f);
			const float x = r * cos(Phi) * sin(Theta);
			const float y = r * sin(Phi) * sin(Theta);
			const float z = r * cos(Theta);
			FVector Point{x, y , z + TrunkHeight};
			AttractionPoints.Add(Point);
		}
	}
}

TArray<FBud> TreeGenerator::InitializeTrunk() const
{
	FTreeNode RootNode = FTreeNode();
	TreeStructure->Nodes.Add(RootNode);
	int Direction = 1;
	float SegmentHeight = TreeProperties->TrunkHeight / TreeProperties->TrunkSegments;
	TArray<FBud> Buds = TArray<FBud>();
	for (int i = 1; i <= TreeProperties->TrunkSegments; i++)
	{
		FTreeNode TrunkNode = FTreeNode();
		TrunkNode.Position = FVector(RootNode.Position.X + (SegmentHeight * TreeProperties->TrunkXDeviation * Direction), 
			RootNode.Position.Y + (SegmentHeight * TreeProperties->TrunkYDeviation * Direction), 
			RootNode.Position.Z + SegmentHeight);
		TrunkNode.Orientation = FRotationMatrix::MakeFromX(FVector::UpVector).ToQuat();
		TrunkNode.ParentIndex = TreeStructure->Nodes.Num() - 1;
		FBranchEdge BranchEdge = FBranchEdge();
		BranchEdge.NodeStart = TreeStructure->Nodes.Num() - 1;
		BranchEdge.NodeEnd = TreeStructure->Nodes.Num();
		TreeStructure->Nodes.Add(TrunkNode);
		TreeStructure->Edges.Add(BranchEdge);

		FBud TrunkBud{RootNode};
		TrunkBud.BudIndex = TreeStructure->Nodes.Num() - 1;
		Buds.Add(TrunkBud);
		
		RootNode = TrunkNode;
		Direction *= -1;
	}
	
	return Buds;
}

void TreeGenerator::GenerateNodes(int Iteration, TArray<FBud>& Buds)
{
	if (Iteration >= TreeProperties->MaxIteration)
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
			float ConeRadius = TreeProperties->PerceptionLength * tan(  FMath::DegreesToRadians(TreeProperties->PerceptionAngle / 2.0f));
			float ConeDist = FVector::DotProduct(Point - Bud.Position, Bud.Orientation.GetForwardVector());
			if (ConeDist < 0 || ConeDist > TreeProperties->PerceptionLength)
				continue;

			float RadiusAtPoint = (ConeDist / TreeProperties->PerceptionLength) * ConeRadius;
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
	float Q = ComputeLightExposure(Buds, AttractionPointsPerBud, QSplit);
	DistributeVigor(Q, Buds, QSplit);
	
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
			FVector Tropism = TreeProperties->TropismWeight * FVector::UpVector;
			AveragePoint = (AveragePoint + Tropism).GetSafeNormal();
			FVector ChildPosition = ParentPosition + BranchLength * AveragePoint;
			ChildNode.Position = ChildPosition;
			ChildNode.Orientation = FRotationMatrix::MakeFromX(AveragePoint).ToQuat();
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
			FQuat RotationQuat = FQuat(FVector::CrossProduct(GrowthDirection, FVector::UpVector).GetSafeNormal(), 
				FMath::DegreesToRadians(TreeProperties->BranchAngle));
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
			if (FVector::DistSquared(Point, Bud.Position) < FMath::Square(TreeProperties->OccupancyRadius))
				PointsToRemove.Add(Point);
		}
	}
	
	for (auto PointToRemove : PointsToRemove)
		AttractionPoints.Remove(PointToRemove);
	
	if (AttractionPoints.Num() == 0)
		return;
	
	GenerateNodes(Iteration + 1, NewBuds);
}

// Accumulates a Q value for each bud in the structure going from tips to root (basipetal).
// At branching points values Q_m (main axis) and Q_l (lateral axis) are calculated and stored for the vigor distribution
float TreeGenerator::ComputeLightExposure(TArray<FBud>& Buds, const TMap<int, TArray<FVector>>& AttractionPointsPerBud,
	TMap<int, TPair<float, float>>& OutQSplit)
{
	TMap<int, TArray<int>> ChildrenOf = GetNodesSplitByChildren();

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

// Starting from the root's Q value, compute and distribute the vigor to each bud, going from root to tip (acropetal)
void TreeGenerator::DistributeVigor(const float QBase, TArray<FBud>& Buds, TMap<int, TPair<float, float>>& QSplit)
{
	float VBase = QBase * TreeProperties->ResourceCoefficient;

	TMap<int, TArray<int>> ChildrenOf = GetNodesSplitByChildren();

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
				Vm = v * (TreeProperties->ApicalControl * Qm) / (TreeProperties->ApicalControl * Qm + (1 - TreeProperties->ApicalControl) * Ql);

			float Vl = v - Vm;
			Vigor.Add(Children[0], Vm);
			for (int j = 1; j < Children.Num(); j++)
				Vigor.Add(Children[j], Vl / (Children.Num() - 1));
		}
	}

	for (auto& Bud : Buds)
		Bud.Vigor = Vigor.FindOrAdd(Bud.BudIndex);
}

// Compute the radius of each node using the Pipe Model
void TreeGenerator::ComputeRadii()
{
	TMap<int, TArray<int>> ChildrenOf = GetNodesSplitByChildren();
	
	for (auto& Node : TreeStructure->Nodes)
		Node.Radius = 0.25f; // initial radius for terminal nodes
	
	for (int i = TreeStructure->Nodes.Num() - 1; i >= 0; i--)
	{
		if (!ChildrenOf.Contains(i))
			continue;

		const TArray<int>& Children = ChildrenOf[i];
		float RadiusSumSquared = pow(TreeStructure->Nodes[Children[0]].Radius, TreeProperties->BranchRadiusExponent);
		for (int j = 1; j < Children.Num(); j++)
			RadiusSumSquared += pow(TreeStructure->Nodes[Children[j]].Radius, TreeProperties->BranchRadiusExponent);
			
		TreeStructure->Nodes[i].Radius = pow(RadiusSumSquared, 1.0f/TreeProperties->BranchRadiusExponent);
	}

	FTreeNode RootNode = TreeStructure->Nodes[0];
	float MaxHeight = TreeProperties->CrownSize + TreeProperties->TrunkHeight;
	for (int i = 0; i < TreeStructure->Nodes.Num(); i++)
	{
		FTreeNode& Node = TreeStructure->Nodes[i];
		const float Height = FVector::Distance(RootNode.Position, Node.Position);
		const float Factor = 1.0f + TreeProperties->TrunkFlareStrength * FMath::Exp(-TreeProperties->TrunkFlareDecay * (Height / MaxHeight));
		Node.Radius *= Factor;
	}
}

void TreeGenerator::DecimateNodes()
{
	TMap<int, TArray<int>> ChildrenOf = GetNodesSplitByChildren();
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

void TreeGenerator::RelocateNodesTowardsParent() const
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

// Subdivide tree branches using Chaikin's algorithm for curves
void TreeGenerator::Subdivide() const
{
	TMap<int, TArray<int>> ChildrenOf = GetNodesSplitByChildren();
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

TMap<int, TArray<int>> TreeGenerator::GetNodesSplitByChildren() const
{
	TMap<int, TArray<int>> Children;
	for (const auto& Edge : TreeStructure->Edges)
		Children.FindOrAdd(Edge.NodeStart).Add(Edge.NodeEnd);
	
	return Children;
}
