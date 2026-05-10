#pragma once

#include "TreePropertiesDataAsset.h"
#include "TreeStructureDataAsset.h"

class TreeGenerator
{
public:
	TreeGenerator(UTreePropertiesDataAsset* TreePropertiesData, UTreeStructureDataAsset* TreeStructureData);
	
	UTreeStructureDataAsset* GenerateTreeStructure();
private:
	UTreePropertiesDataAsset* TreeProperties;
	UTreeStructureDataAsset* TreeStructure;
	FRandomStream RandomStream;
	TArray<FVector> AttractionPoints;
	TMap<int, float> NodesLifeTimeLightAccumulation;
	
	void GenerateAttractionPoints();
	void SeedPointsInSphere();
	void SeedPointsInCylinder();
	void SeedPointsInCone();
	TArray<FBud> InitializeTrunk() const;
	void GenerateNodes(int Iteration, TArray<FBud>& Buds);
	float ComputeLightExposure(TArray<FBud>& Buds, const TMap<int, TArray<FVector>>& AttractionPointsPerBud, 
								TMap<int, TPair<float, float>>& OutQSplit);
	void DistributeVigor(float QBase, TArray<FBud>& Buds, TMap<int, TPair<float, float>>& QSplit);
	void ComputeRadii();
	void DecimateNodes();
	void RelocateNodesTowardsParent() const;
	void Subdivide() const;
	void BranchShedding();
	
	TMap<int, TArray<int>> GetNodesSplitByChildren() const;
	void RemapNodes(const TSet<int>& NodesToKeep, TArray<FTreeNode>& OutNodes, TMap<int, int>& OutRemappedIndices) const;
	TArray<int> WalkChain(int Start, int End, const TSet<int>& Stops, const TMap<int, TArray<int>>& ChildrenOf) const;
	TSet<int> CollectTipAndBranchingNodes(const TMap<int, TArray<int>>& ChildrenOf) const;
};
