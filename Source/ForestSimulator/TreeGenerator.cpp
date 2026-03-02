#include "TreeGenerator.h"
#include "TreeStructureDataAsset.h"
#include "TreePropertiesDataAsset.h"

void UTreeGenerator::GenerateTreeStructure(UTreePropertiesDataAsset* TreePropertiesData, UTreeStructureDataAsset* TreeStructureData)
{
	TreeStructureData->Edges.Empty();
	TreeStructureData->Nodes.Empty();
	TreeStructureData->Leaves.Empty();
	TreeStructureData->Modules.Empty();
	
	FTurtleFrame TurtleFrame;
	TurtleFrame.Position = FVector::ZeroVector;
	TurtleFrame.Frame = FQuat::Identity;
	FBranchModule BranchModule;
	TreeStructureData->Modules.Add(BranchModule);
	FTreeNode StartTreeNode;
	StartTreeNode.Position = TurtleFrame.Position;
	StartTreeNode.Orientation = TurtleFrame.Frame;
	StartTreeNode.Radius = TreePropertiesData->TrunkRadius;
	TreeStructureData->Nodes.Add(StartTreeNode);

	FRandomStream Random{TreePropertiesData->RandomSeed};
	
	GenerateStructure(Random, TreePropertiesData, TreeStructureData, TurtleFrame, TreePropertiesData->TrunkRadius, 0, 0);
}

void UTreeGenerator::GenerateStructure(FRandomStream& RandomStream, UTreePropertiesDataAsset* TreeData, UTreeStructureDataAsset* TreeStructure,
	FTurtleFrame TurtleFrame, float BranchRadius, int ModuleIndex, int PreviousNodeIndex)
{
	FVector Axis = FVector::CrossProduct(TurtleFrame.Frame.GetUpVector(), FVector::DownVector);
	if (!Axis.IsNearlyZero())
	{
		FQuat Rotation = FQuat(Axis.GetSafeNormal(), FMath::DegreesToRadians(TreeData->TropismStrength * RandomStream.FRandRange(0.85f, 1.15f)));
		TurtleFrame.Frame = Rotation * TurtleFrame.Frame;
	}
	
	float Length = BranchRadius * TreeData->SegmentLengthFactor * RandomStream.FRandRange(0.85f, 1.15f);
	FTreeNode EndTreeNode;
	TurtleFrame.Position = TurtleFrame.Position + TurtleFrame.Frame.GetUpVector() * Length;
	EndTreeNode.Position = TurtleFrame.Position;
	EndTreeNode.Orientation = TurtleFrame.Frame;
	EndTreeNode.Radius = BranchRadius;
	EndTreeNode.ParentIndex = PreviousNodeIndex;
	
	FBranchEdge BranchEdge;
	BranchEdge.NodeStart = PreviousNodeIndex;
	TreeStructure->Nodes.Add(EndTreeNode);
	int NewEndIndex = TreeStructure->Nodes.Num() - 1;
	BranchEdge.NodeEnd = NewEndIndex;
	BranchEdge.Length = Length;
	BranchEdge.Mass = PI * BranchRadius * BranchRadius * Length * TreeData->WoodDensity;
	BranchEdge.Thickness = BranchRadius;
	TreeStructure->Edges.Add(BranchEdge);

	TreeStructure->Modules[ModuleIndex].EdgeIndices.Add(TreeStructure->Edges.Num() - 1);
	TreeStructure->Modules[ModuleIndex].Mass += BranchEdge.Mass;
	
	if (BranchRadius < TreeData->MinRadius)
	{
		FLeafInstance LeafInstance;
		LeafInstance.AttachedNodeIndex = BranchEdge.NodeEnd;
		TreeStructure->Leaves.Add(LeafInstance);
		return;
	}
	
	for (int i = 0; i < TreeData->BranchCount; i++)
	{
		if (RandomStream.FRandRange(0.0f, 1.0f) > 0.8f)
			continue;
		
		float Radius = BranchRadius / FMath::Sqrt(static_cast<float>(TreeData->BranchCount));
		int NewModuleIndex;
		FTurtleFrame ChildTurtle;
		FQuat Pitch;
		FQuat Yaw;
		FQuat Roll = FQuat::Identity;
		if (TreeData->Sympodial)
		{
			NewModuleIndex = TreeStructure->Modules.Num();
			FBranchModule BranchModule;
			BranchModule.ParentModuleIndex = ModuleIndex;
			TreeStructure->Modules.Add(BranchModule);
			
			Pitch = FQuat(TurtleFrame.Frame.GetRightVector(), FMath::DegreesToRadians(TreeData->BranchAngle + TreeData->BranchAngle * RandomStream.FRandRange(0.1f, 0.2f)));
			Yaw = FQuat(TurtleFrame.Frame.GetUpVector(), FMath::DegreesToRadians(i * (360.0f / TreeData->BranchCount)));
			Roll = FQuat(TurtleFrame.Frame.GetForwardVector(), FMath::DegreesToRadians(i * TreeData->PhyllotaxisAngle));
		}
		else
		{
			if (i == 0)
			{
				NewModuleIndex = ModuleIndex;
				Pitch = FQuat(TurtleFrame.Frame.GetRightVector(), FMath::DegreesToRadians(TreeData->ApicalCurvature));
				Yaw = FQuat(TurtleFrame.Frame.GetUpVector(), FMath::DegreesToRadians(RandomStream.FRandRange(5.0f, 10.0f)));
			}
			else
			{
				NewModuleIndex = TreeStructure->Modules.Num();
				FBranchModule BranchModule;
				BranchModule.ParentModuleIndex = ModuleIndex;
				TreeStructure->Modules.Add(BranchModule);
				
				int j = i - 1;
				Pitch = FQuat(TurtleFrame.Frame.GetRightVector(), FMath::DegreesToRadians(TreeData->BranchAngle + TreeData->BranchAngle * RandomStream.FRandRange(0.1f, 0.2f)));
				Yaw = FQuat(TurtleFrame.Frame.GetUpVector(), FMath::DegreesToRadians(j * (360.0f / (TreeData->BranchCount - 1))));
				Roll = FQuat(TurtleFrame.Frame.GetForwardVector(), FMath::DegreesToRadians(j * TreeData->PhyllotaxisAngle));
			}
		}

		ChildTurtle.Position = TurtleFrame.Position;
		ChildTurtle.Frame = Pitch * Yaw * Roll * TurtleFrame.Frame;

		GenerateStructure(RandomStream, TreeData, TreeStructure, ChildTurtle, Radius, NewModuleIndex, NewEndIndex);
	}
}


