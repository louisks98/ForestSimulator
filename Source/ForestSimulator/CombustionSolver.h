// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tree.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraDataChannelPublic.h"
#include "NiagaraComponent.h"
#include "GameFramework/Actor.h"
#include "CombustionSolver.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FORESTSIMULATOR_API UCombustionSolver : public UActorComponent
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UTextureRenderTarget* RenderTarget;
	
	UCombustionSolver();
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	
	UFUNCTION(BlueprintCallable)
	void Initialize(ATree* TreeRef, UNiagaraComponent* NiagaraComponent);
	UFUNCTION(BlueprintCallable)
	void IgniteFire();
private:
	UPROPERTY()
	ATree* Tree;
	UPROPERTY()
	UNiagaraComponent* Simulation;
	
	const float WaterEvaporationRate = 0.0003f;
	const float MinCombustionTemperature = 150.0f;
	const float MaxCombustionTemperature = 450.0f;
	const float HeatGenerationRate = 0.0000012f;
	const float	SmokeFromWaterEvaporation = 200;
	
	const int LogInterval = 60;
	int FrameCounter;
	
	void Solve(float DeltaTime);
	float GetAirTemperature(const FVector& Position, TArray<FFloat16Color>& RTPixels) const;
	void FeedFireSimulation(const TArray<FVector>& Positions, const TArray<float>& Temperatures, const TArray<float>& SmokeDensities) const;

	static float LaplacianOperator(const FBranchEdge* Edge, const FBranchEdge* Parent, TArray<FBranchEdge*>& EdgeChildren);
};
