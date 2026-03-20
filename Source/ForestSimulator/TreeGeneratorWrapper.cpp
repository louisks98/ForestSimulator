#include "TreeGeneratorWrapper.h"
#include "TreeGenerator.h"

void UTreeGeneratorWrapper::GenerateTreeStructure(UTreePropertiesDataAsset* TreePropertiesData, UTreeStructureDataAsset* TreeStructureData)
{
	double StartTime = FPlatformTime::Seconds();
	
	TreeGenerator TreeGenerator(TreePropertiesData, TreeStructureData);
	TreeGenerator.GenerateTreeStructure();
	
	double EndTime = FPlatformTime::Seconds();
	double ElapsedTime = EndTime - StartTime;
	UE_LOG(LogTemp, Warning, TEXT("Tree graph generation execution time: %f seconds. Generated %d nodes and %d edges"), 
		ElapsedTime, TreeStructureData->Nodes.Num(), TreeStructureData->Edges.Num());
}