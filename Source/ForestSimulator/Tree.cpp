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
		TreeNodes      = TreeData->Nodes;
		BranchEdges    = TreeData->Edges;
		BranchModules  = TreeData->Modules;
		LeafInstances  = TreeData->Leaves;
		
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
	for (int i = 0; i < BranchEdges.Num(); i++)
	{
		FTreeNode Start = TreeNodes[BranchEdges[i].NodeStart];
		FTreeNode End = TreeNodes[BranchEdges[i].NodeEnd];
		BuildBranchMesh(i, Start, End);
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
}

void ATree::BuildBranchMesh(int Index, FTreeNode Start, FTreeNode End) const
{
	TArray<int32> Indices = TArray<int32>();
	TArray<FVector> Vertices = TArray<FVector>();
	TArray<FVector> Normals = TArray<FVector>();
	TArray<FVector2D> UVs = TArray<FVector2D>();
	TArray<FColor> Colors = TArray<FColor>();
	
	int Resolution = FMath::Clamp(FMath::RoundToInt(Start.Radius), MinNumSides, MaxNumSides);
	FVector BranchAxis = (Start.Position - End.Position).GetSafeNormal();
	FVector Ref = FMath::Abs(FVector::DotProduct(BranchAxis, FVector::UpVector)) < 0.95f ? FVector::UpVector : FVector::ForwardVector;
	FVector Right = FVector::CrossProduct(BranchAxis, Ref).GetSafeNormal();
	FVector Forward = FVector::CrossProduct(Right, BranchAxis).GetSafeNormal();
	
	for (int i = 0; i < Resolution; ++i)
	{
		float Angle = i * (2 * PI / Resolution);
		FVector StartOffset = cos(Angle) * Right + sin(Angle) * Forward;
		Vertices.Add(Start.Position + StartOffset * Start.Radius);
		Normals.Add(StartOffset);
		UVs.Add(FVector2D(i / static_cast<float>(Resolution), 0.0f));
		Colors.Add(FColor(0,0,0));
	}
	
	for (int i = 0; i < Resolution; ++i)
	{
		float Angle = i * (2 * PI / Resolution);
		FVector EndOffset = cos(Angle) * Right + sin(Angle) * Forward;
		Vertices.Add(End.Position + EndOffset * End.Radius);
		Normals.Add(EndOffset);
		UVs.Add(FVector2D(i / static_cast<float>(Resolution), 1.0f));
		Colors.Add(FColor(0,0,0));
	}

	for (int i = 0; i < Resolution; i++)
	{
		int A = i;
		int B = (i + 1) % Resolution;
		int C = Resolution + i;
		int D = Resolution + (i + 1) % Resolution;

		Indices.Add(A);
		Indices.Add(C);
		Indices.Add(B);

		Indices.Add(C);
		Indices.Add(D);
		Indices.Add(B);
	}

	MeshComponent->CreateMeshSection(Index, Vertices, Indices, Normals, UVs, Colors, TArray<FProcMeshTangent>{}, false);
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

