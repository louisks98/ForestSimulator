#include "Tree.h"
#include "TreeStructureDataAsset.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

ATree::ATree()
{
	PrimaryActorTick.bCanEverTick = true;
	MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>("MeshComponent");
	MeshComponent->bUseComplexAsSimpleCollision = false;
	LeavesInstanceComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>("LeavesInstanceComponent");
}

void ATree::BeginPlay()
{
	Super::BeginPlay();

	if (TreeData)
	{
		TreeNodes = TreeData->Nodes;
		BranchEdges = TreeData->Edges;
		BranchModules = TreeData->Modules;
		LeafInstances = TreeData->Leaves;
		
		UE_LOG(LogTemp, Log,
		       TEXT("ATree '%s': loaded %d nodes, %d edges, %d modules, %d leaves."),
		       *GetName(),
		       TreeNodes.Num(), BranchEdges.Num(),
		       BranchModules.Num(), LeafInstances.Num());

		if(LeafMesh)
			LeavesInstanceComponent->SetStaticMesh(LeafMesh);

		if (RenderBranches)
			BuildTreeMesh();
		
		if (Debug)
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

void ATree::BuildTreeMesh()
{
	double StartTime = FPlatformTime::Seconds();
	TMap<int, TArray<int>> ChildrenOf;
	for (const auto& Edge : BranchEdges)
		ChildrenOf.FindOrAdd(Edge.NodeStart).Add(Edge.NodeEnd);
	
	TSet<int> BranchingNodes;
	TSet<int> RenderedNodes;
	for (int i = 0; i < TreeNodes.Num(); i++)
	{
		if (!ChildrenOf.Contains(i))
			continue;
		
		if (ChildrenOf.Contains(i) && ChildrenOf[i].Num() > 1)
		{
			BranchingNodes.Add(i);
			continue;
		}
		
		int Parent = TreeNodes[i].ParentIndex;
		if (Parent == INDEX_NONE)
			BranchingNodes.Add(i);
	}
	
	int MeshIndex = 0;
	for (int i = 0; i < BranchEdges.Num(); i++)
	{
		int Start = BranchEdges[i].NodeStart;
		int End = BranchEdges[i].NodeEnd;
		
		if (!BranchingNodes.Contains(Start))
			continue;
		
		TArray<int> Chain;
		if (!RenderedNodes.Contains(Start))
			Chain.Add(Start);
		
		while (ChildrenOf.Contains(End))
		{
			if (!RenderedNodes.Contains(End))
				Chain.Add(End);
			
			int Index = ChildrenOf[End][0];
			if (BranchingNodes.Contains(End) && ChildrenOf[End].Num() > 1)
			{
				FQuat ParentOrientation = TreeNodes[End].Orientation;
				FVector ParentForward = ParentOrientation.GetForwardVector();
				float Deviation = -MAX_flt;
				for (int j = 0; j < ChildrenOf[End].Num(); ++j)
				{
					FQuat ChildOrientation = TreeNodes[ChildrenOf[End][j]].Orientation;
					FVector ChildForward = ChildOrientation.GetForwardVector();
					
					float Dot = ParentForward.Dot(ChildForward);
					if (Dot > Deviation)
					{
						Index = ChildrenOf[End][j];
						Deviation = Dot;
					}
				}
			}
			End = Index;
		}
		if (!RenderedNodes.Contains(End))
			Chain.Add(End);
		
		if (Chain.Num() > 1)
		{
			BuildBranchMesh(MeshIndex, Chain);
			RenderedNodes.Append(Chain);
			MeshIndex++;
		}
	}
	MeshComponent->UpdateBounds();

	if (!RenderLeaves)
		return;
	
	for (int i = 0; i < LeafInstances.Num(); i++)
	{
		FLeafInstance Leaf = LeafInstances[i];
		FTreeNode Node = TreeNodes[Leaf.AttachedNodeIndex];
		FTransform WorldPosition = GetActorTransform();
		LeafRotations.Add(WorldPosition.TransformRotation(Node.Orientation));
		LeavesInstanceComponent->AddInstance(FTransform(WorldPosition.TransformRotation(Node.Orientation),
			WorldPosition.TransformPosition(Node.Position)), true);
	}

	double EndTime = FPlatformTime::Seconds();
	double ElapsedTime = EndTime - StartTime;
	UE_LOG(LogTemp, Warning, TEXT("Tree mesh generation execution time: %f seconds."), ElapsedTime);
}

void ATree::BuildBranchMesh(int Index, TArray<int>& Chain)
{
	TArray<int32> Indices = TArray<int32>();

	FTreeNode Start = TreeNodes[Chain[0]];
	FTreeNode End = TreeNodes[Chain[1]];
	FVector PrevAxis = (Start.Position - End.Position).GetSafeNormal();
	FVector Ref = FMath::Abs(FVector::DotProduct(PrevAxis, FVector::UpVector)) < 0.95f ? FVector::UpVector : FVector::ForwardVector;
	FVector PrevRight = FVector::CrossProduct(PrevAxis, Ref).GetSafeNormal();
	FVector PrevForward = FVector::CrossProduct(PrevRight, PrevAxis).GetSafeNormal();
	int Resolution = FMath::Clamp(FMath::RoundToInt(Start.Radius), MinNumSides, MaxNumSides);
	
	Vertices Vertices = ComputeVertices(Resolution, Chain[0], PrevRight, PrevForward, 0.0f);
	Vertices.Append(ComputeVertices(Resolution, Chain[1], PrevRight, PrevForward, 1.0f));
	Indices.Append(ComputeIndices(Resolution, 0));
	
	for (int i = 1; i < Chain.Num() - 1; i++)
	{
		Start = TreeNodes[Chain[i]];
		End = TreeNodes[Chain[i + 1]];
		FVector Axis = (Start.Position - End.Position).GetSafeNormal();
		FQuat Rotation = FQuat::FindBetweenNormals(PrevAxis, Axis);
		FVector Right = Rotation.RotateVector(PrevRight);
		FVector Forward = Rotation.RotateVector(PrevForward);
		
		Vertices.Append(ComputeVertices(Resolution, Chain[i + 1], Right, Forward, 1.0f));
		Indices.Append(ComputeIndices(Resolution, Vertices.Positions.Num() - 2 * Resolution));
		
		PrevRight = Right;
		PrevForward = Forward;
		PrevAxis = Axis;
	}
	
	MeshComponent->CreateMeshSection(Index, Vertices.Positions, Indices, Vertices.Normals, Vertices.UVs, Vertices.Colors, TArray<FProcMeshTangent>{}, false);
}

Vertices ATree::ComputeVertices(int Resolution, int NodeIndex, FVector Right, FVector Forward, float UV_v)
{
	Vertices Vertices;
	const FTreeNode Node = TreeNodes[NodeIndex];
	for (int i = 0; i < Resolution; ++i)
	{
		const float Angle = i * (2 * PI / Resolution);
		FVector Offset = cos(Angle) * Right + sin(Angle) * Forward;
		const float Noise = FMath::PerlinNoise1D(NodeIndex + (i * 137.5f));
		const float DeviatedRadius = Node.Radius * (1.0f + MeshNoiseStrength * Noise);
		Vertices.Positions.Add(Node.Position + Offset * DeviatedRadius);
		Vertices.Normals.Add(Offset);
		Vertices.UVs.Add(FVector2D(i / static_cast<float>(Resolution), UV_v));
		Vertices.Colors.Add(FColor(0,0,0));
	}
	
	return Vertices;
}

TArray<int> ATree::ComputeIndices(int Resolution, int Offset)
{
	TArray<int> Indices;
	for (int j = 0; j < Resolution; j++)
	{
		int A = Offset + j;
		int B = Offset + (j + 1) % Resolution;
		int C = Offset + Resolution + j;
		int D = Offset + Resolution + (j + 1) % Resolution;

		Indices.Add(A);
		Indices.Add(C);
		Indices.Add(B);

		Indices.Add(C);
		Indices.Add(D);
		Indices.Add(B);
	}
	
	return Indices;
}

void ATree::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->PlayerCameraManager)
		return;

	FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();

	for (int i = 0; i < LeavesInstanceComponent->GetInstanceCount(); i++)
	{
		FTransform InstanceTransform;
		LeavesInstanceComponent->GetInstanceTransform(i, InstanceTransform, true);
		FVector ToCamera = CameraLocation - InstanceTransform.GetLocation();
		ToCamera.Z = 0.0f;

		if (!ToCamera.IsNearlyZero())
		{
			FRotator LookAtRotation = FRotationMatrix::MakeFromX(ToCamera).Rotator();
			LookAtRotation.Pitch = 0.0f;
			LookAtRotation.Roll = 0.0f;
			FQuat YawQuat = LookAtRotation.Quaternion();
			InstanceTransform.SetRotation(YawQuat * LeafRotations[i]);
			LeavesInstanceComponent->UpdateInstanceTransform(i, InstanceTransform, true, false);
		}
	}
}

