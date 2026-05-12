#include "CombustionSolver.h"

ACombustionSolver::ACombustionSolver()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACombustionSolver::BeginPlay()
{
	Super::BeginPlay();
}

void ACombustionSolver::Solve(float DeltaTime)
{
	for (int i = 0; i < Trees.Num(); ++i)
	{
		ATree* Tree = Trees[i];
		for (int j = 0; j < Tree->EdgeCount(); ++j)
		{
			FBranchEdge* Edge = Tree->GetEdge(j);
			FBranchEdge* Parent = Tree->GetParentEdge(j);
			TArray<FBranchEdge*> EdgeChildren = Tree->GetChildEdges(j);
			float Laplacian = LaplacianOperator(Edge, Parent, EdgeChildren);
			
			FVector StartPosition = Tree->GetNode(Edge->NodeStart)->Position;
			FVector EndPosition = Tree->GetNode(Edge->NodeEnd)->Position;
			
			float AirTemperature = GetAirTemperature(FMath::Lerp(StartPosition, EndPosition, 0.5));
			float Moisture = (1 - Edge->WaterContent) * Tree->WoodProperties.DryWoodCoefficient + Edge->WaterContent * Tree->WoodProperties.WetWoodCoefficient;
			float SurfaceTemperature = Edge->Temperature + DeltaTime * (Tree->WoodProperties.TemperatureDiffusion * Laplacian + Moisture * (AirTemperature - Edge->Temperature));
			float WaterContent = Edge->WaterContent + DeltaTime * -WaterEvaporationRate * SurfaceTemperature;
			
			float Thickness = pow(2 * (Edge->Mass/Tree->WoodProperties.Density) * (Edge->InitialThickness / Edge->InitialArea) ,0.5);
			float CharThickness = Tree->WoodProperties.ContractionFactor * (Edge->InitialThickness - Thickness);
			
			float CharInsulation = Tree->WoodProperties.MinimumValueCharring + (1 - Tree->WoodProperties.MinimumValueCharring) * pow(EULERS_NUMBER, - Tree->WoodProperties.RateCharInsulation * CharThickness);
			float Area = Edge->InitialArea * Thickness/Edge->InitialThickness;
			float ReactionRateCoefficient;
			if (SurfaceTemperature < MinCombustionTemperature)
				ReactionRateCoefficient = 0.0f;
			else if (SurfaceTemperature > MaxCombustionTemperature)
				ReactionRateCoefficient = 1.0f;
			else
				ReactionRateCoefficient = 3 * pow(SurfaceTemperature, 2) * 2 * pow(SurfaceTemperature, 3);
			
			float Mass = Edge->Mass + DeltaTime * -ReactionRateCoefficient * CharInsulation * Area;
			
			float SmokeDensity = Tree->WoodProperties.AmountOfSmokeFromBurning * Mass - SmokeFromWaterEvaporation * WaterContent;
			float HeatTransfer = -HeatGenerationRate * Mass;
			
			// Todo: feed HeatTransfer and SmokeDensity to fire sim
			
			Edge->Temperature = SurfaceTemperature;
			Edge->WaterContent = WaterContent;
			Edge->Thickness = Thickness;
			Edge->CharInsulation = CharInsulation;
			Edge->Area = Area;
			Edge->Mass = Mass;
		}
	}
}

float ACombustionSolver::GetAirTemperature(FVector Position)
{
	// Todo Read Air temperature from fire sim.
	return 20.0;
}

float ACombustionSolver::LaplacianOperator(FBranchEdge* Edge, FBranchEdge* Parent, TArray<FBranchEdge*>& EdgeChildren)
{
	float Sum = Parent->Temperature - Edge->Temperature;
	for (auto Child : EdgeChildren)
		Sum += Child->Temperature - Edge->Temperature;
	
	return Sum;
}

void ACombustionSolver::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

