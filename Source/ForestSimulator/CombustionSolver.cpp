#include "CombustionSolver.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "CompGeom/FitOrientedBox2.h"
#include "Engine/TextureRenderTarget2D.h"

UCombustionSolver::UCombustionSolver()
{
	PrimaryComponentTick.bCanEverTick = true;
	FrameCounter = 0;
}

void UCombustionSolver::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Solve(DeltaTime);
}


void UCombustionSolver::Initialize(ATree* TreeRef, UNiagaraComponent* NiagaraComponent)
{
	if (TreeRef)
		Tree = TreeRef;
	else
		UE_LOG(LogTemp, Error, TEXT("No ATree found on %s. "), *GetOwner()->GetName());
	
	if (RenderTarget)
	{
		if (NiagaraComponent)
		{
			Simulation = NiagaraComponent;
			Simulation->SetVariableTextureRenderTarget(FName("ProbeRT"), RenderTarget);
		}
		else
			UE_LOG(LogTemp, Error, TEXT("No UNiagaraComponent found on %s. "), *GetOwner()->GetName());
	}
	else
		UE_LOG(LogTemp, Warning, TEXT("No RenderTarget assigned."));
}

void UCombustionSolver::IgniteFire()
{
	const int Index = FMath::RandRange(0, Tree->EdgeCount() - 1);
	FBranchEdge* Edge = Tree->GetEdge(Index);
	Edge->Temperature = 500.0f;
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, "Fire ignited");
}

void UCombustionSolver::Solve(float DeltaTime)
{
	FTextureRenderTargetResource* RTResource =
	RenderTarget->GameThread_GetRenderTargetResource();
	TArray<FFloat16Color> Float16Pixels;
	RTResource->ReadFloat16Pixels(Float16Pixels);

	TArray<FVector> PendingPositions;
	TArray<float> PendingTemperatures;
	TArray<float> PendingSmokeDensities;

	if (!Tree)
		return;

		for (int j = 0; j < Tree->EdgeCount(); ++j)
		{
			FBranchEdge* Edge = Tree->GetEdge(j);
			FBranchEdge* Parent = Tree->GetParentEdge(j);
			if (Parent == nullptr)
				continue;

			if (Edge->Temperature >= 500.0f)
				bool Debug = true;
			
			TArray<FBranchEdge*> EdgeChildren = Tree->GetChildEdges(j);
			float Laplacian = LaplacianOperator(Edge, Parent, EdgeChildren);

			FVector StartPosition = Tree->GetNode(Edge->NodeStart)->Position;
			FVector EndPosition = Tree->GetNode(Edge->NodeEnd)->Position;
			FVector Position = FMath::Lerp(StartPosition, EndPosition, 0.5);

			float AirTemperature = GetAirTemperature(Position, Float16Pixels);
			float Moisture = (1 - Edge->WaterContent) * Tree->WoodProperties.DryWoodCoefficient + Edge->WaterContent * Tree->WoodProperties.WetWoodCoefficient;
			float SurfaceTemperature = Edge->Temperature + DeltaTime * (Tree->WoodProperties.TemperatureDiffusion * Laplacian + Moisture * (AirTemperature - Edge->Temperature));
			float WaterContentRate = -WaterEvaporationRate * SurfaceTemperature;
			float WaterContent = FMath::Clamp(Edge->WaterContent + DeltaTime * WaterContentRate, 0, 1);

			float Thickness = pow(2 * (Edge->Mass/Tree->WoodProperties.Density) * (Edge->InitialThickness / Edge->InitialArea) ,0.5);
			float CharThickness = Tree->WoodProperties.ContractionFactor * (Edge->InitialThickness - Thickness);

			float CharInsulation = Tree->WoodProperties.MinimumValueCharring + (1 - Tree->WoodProperties.MinimumValueCharring) * pow(EULERS_NUMBER, - Tree->WoodProperties.RateCharInsulation * CharThickness);
			float Area = Edge->InitialArea * Thickness/Edge->InitialThickness;
			float ReactionRateCoefficient;
			if (SurfaceTemperature < MinCombustionTemperature)
				ReactionRateCoefficient = 0.0f;
			else if (SurfaceTemperature > MaxCombustionTemperature)
				ReactionRateCoefficient = Tree->WoodProperties.MaxMassLossRateCoefficient;
			else
			{
				float TempNormalized = (SurfaceTemperature - MinCombustionTemperature) / (MaxCombustionTemperature - MinCombustionTemperature);
				ReactionRateCoefficient = Tree->WoodProperties.MaxMassLossRateCoefficient * (3 * pow(TempNormalized, 2) - 2 * pow(TempNormalized, 3));
			}

			float MassLossRate = -ReactionRateCoefficient * CharInsulation * Area;
			float Mass = Edge->Mass + DeltaTime * MassLossRate;

			float SmokeDensity = -(Tree->WoodProperties.AmountOfSmokeFromBurning * MassLossRate) - (SmokeFromWaterEvaporation * WaterContentRate);
			float HeatTransfer = -HeatGenerationRate * MassLossRate;

			if (HeatTransfer > 0)
			{
				PendingPositions.Add(Position);
				PendingTemperatures.Add(HeatTransfer);
				PendingSmokeDensities.Add(SmokeDensity);
				
				FrameCounter++;
				if (FrameCounter > LogInterval)
				{
					FrameCounter = 0;
					if (GEngine)
						GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Orange,
								FString::Format(TEXT("HeatTransfer {0}"), 
								{HeatTransfer}));
				}
			}

			Edge->Temperature = SurfaceTemperature;
			Edge->WaterContent = WaterContent;
			Edge->Thickness = Thickness;
			Edge->CharInsulation = CharInsulation;
			Edge->Area = Area;
			Edge->Mass = Mass;
		}

	FeedFireSimulation(PendingPositions, PendingTemperatures, PendingSmokeDensities);
	
	
	
}

float UCombustionSolver::GetAirTemperature(const FVector& Position, TArray<FFloat16Color>& RTPixels) const
{
	int NumCells = 128;
	float BoxSize = 600.0;
	int TilePerRow = ceil(sqrt(NumCells)); 
	FVector SimulationPosition = Simulation->GetComponentLocation();
	
	FVector CellPosition = (Position - SimulationPosition) / BoxSize + 0.5;
	FVector2D TilePosition = FVector2D(static_cast<int>(CellPosition.Z) % TilePerRow, CellPosition.Z / TilePerRow);
	int PixelX = TilePosition.X * NumCells + CellPosition.X;
	int PixelY = TilePosition.Y * NumCells + CellPosition.Y;
	
	int width = NumCells * TilePerRow; // numCellsX
	int index = PixelY * width + PixelX;
	
	if (RTPixels.IsValidIndex(index))
	{
		float temp = RTPixels[index].R.GetFloat();
		if (GEngine && temp > 5.0f)
			GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Blue, FString::Format(TEXT("Air Temperature {0}"), {temp}));
		
		return temp;
	}
	
	return 0.0f;
}

void UCombustionSolver::FeedFireSimulation(const TArray<FVector>& Positions, const TArray<float>& Temperatures, const TArray<float>& SmokeDensities) const
{
	Simulation->SetIntParameter(FName(TEXT("User.ParticleCount")), Positions.Num());
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(Simulation, FName(TEXT("User.ParticlePositions")), Positions);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(Simulation, FName(TEXT("User.ParticleTemperatures")), Temperatures);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(Simulation, FName(TEXT("User.ParticleSmokes")), SmokeDensities);
}


float UCombustionSolver::LaplacianOperator(const FBranchEdge* Edge, const FBranchEdge* Parent, TArray<FBranchEdge*>& EdgeChildren)
{
	float Sum = Parent->Temperature - Edge->Temperature;
	for (auto Child : EdgeChildren)
		Sum += Child->Temperature - Edge->Temperature;
	
	return Sum;
}

