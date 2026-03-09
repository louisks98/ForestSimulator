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
	bool NoPointFound = true;
	for (int i = 0; i < Buds.Num(); i++)
	{
		FBud Bud = Buds[i];
		for (const auto& Points : AttractionPoints)
		{
			float ConeRadius = TreeData->PerceptionLength * tan(  FMath::DegreesToRadians(TreeData->PerceptionAngle / 2.0f));
			float ConeDist = FVector::DotProduct(Points - Bud.Position, Bud.Orientation.GetForwardVector());
			if (ConeDist < 0 || ConeDist > TreeData->PerceptionLength)
				continue;

			float RadiusAtPoint = (ConeDist / TreeData->PerceptionLength) * ConeRadius;
			FVector Ortho = (Points - Bud.Position) - ConeDist * Bud.Orientation.GetForwardVector();
			if (Ortho.Length() > RadiusAtPoint)
				continue;
			
			AttractionPointsPerBud.FindOrAdd(i).Add(Points);
			NoPointFound = false;
		}
	}

	if (NoPointFound)
		return;

	TMap<int, TPair<float, float>> QSplit;
	float Q = DoBasipetalPass(TreeStructure, Buds, AttractionPointsPerBud, QSplit);
	DoAcropetalPass(Q, TreeData, TreeStructure, Buds, QSplit);

	int FirstNewNode = TreeStructure->Nodes.Num();
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

		// Todo Add section 6.3 tropism
	}

	for (int i = FirstNewNode; i < TreeStructure->Nodes.Num(); i++)
	{
		FTreeNode Node = TreeStructure->Nodes[i];
		for (const auto& Point : AttractionPoints)
		{
			if (FVector::DistSquared(Point, Node.Position) < FMath::Square(TreeData->OccupancyRadius))
				PointsToRemove.Add(Point);
		}
	}
	
	for (auto PointToRemove : PointsToRemove)
		AttractionPoints.Remove(PointToRemove);
	
	if (AttractionPoints.Num() == 0)
		return;

	for (int i = 0 ; i < Buds.Num(); i++)
	{
		if (!AttractionPointsPerBud.Contains(i))
			NewBuds.Add(Buds[i]);
	}
	
	GenerateNodes(Iterations + 1, TreeData, TreeStructure, AttractionPoints, NewBuds);
}

void UTreeGenerator::GenerateAttractionPoints(FRandomStream& RandomStream, UTreePropertiesDataAsset* TreeData,
	TArray<FVector>& AttractionPoints)
{
	for (int i = 0; i < TreeData->NbAttractionPoints; i++)
	{
		const float TreeHeight = TreeData->TreeHeight;
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
			FVector Point{x, y , z + TreeHeight};
			AttractionPoints.Add(Point);
		}
		else if (TreeData->PointDistribution == EPointDistribution::Uniform)
		{
			const float w = RandomStream.FRand();
			const float r = TreeData->CrownSize * FMath::Pow(w, 1.0f / 3.0f);
			const float x = r * cos(Phi) * sin(Theta);
			const float y = r * sin(Phi) * sin(Theta);
			const float z = r * cos(Theta);
			FVector Point{x, y , z + TreeHeight};
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
	for (const auto PointPerBud : AttractionPointsPerBud)
	{
		FBud Bud = Buds[PointPerBud.Key];
		QValues.FindOrAdd(Bud.BudIndex) += PointPerBud.Value.Num();
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

void UTreeGenerator::DoAcropetalPass(int QBase, UTreePropertiesDataAsset* TreeData, UTreeStructureDataAsset* TreeStructure, TArray<FBud>& Buds, TMap<int, TPair<float, float>>& QSplit)
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




