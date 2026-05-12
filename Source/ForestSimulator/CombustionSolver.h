// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tree.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "GameFramework/Actor.h"
#include "CombustionSolver.generated.h"

UCLASS()
class FORESTSIMULATOR_API ACombustionSolver : public AActor
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	TArray<ATree*> Trees;
	UPROPERTY()
	TArray<UNiagaraSystem*> FireSimulators;
	
	ACombustionSolver();
	virtual void Tick(float DeltaTime) override;
	
	
protected:
	virtual void BeginPlay() override;
	
private:
	const float WaterEvaporationRate = 0.0003f;
	const float MinCombustionTemperature = 150.0f;
	const float MaxCombustionTemperature = 450.0f;
	const float HeatGenerationRate = 0.0000012f;
	const float	SmokeFromWaterEvaporation = 200;
	
	void Solve(float DeltaTime);
	float GetAirTemperature(FVector Position);
	
	float LaplacianOperator(FBranchEdge* Edge, FBranchEdge* Parent, TArray<FBranchEdge*>& EdgeChildren);
};
