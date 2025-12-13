// ToggleableDebugVisualization.h

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ToggleableDebugVisualization.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UToggleableDebugVisualization : public UInterface
{
	GENERATED_BODY()
};

class GENGINE3ALEKSEI_API IToggleableDebugVisualization
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void ToggleDebugVisualization();
};
