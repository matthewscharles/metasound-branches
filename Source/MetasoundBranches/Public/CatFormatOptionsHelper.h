#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/PropertyText.h"

#include "CatFormatOptionsHelper.generated.h"

UCLASS()
class METASOUNDBRANCHES_API UCatFormatOptionsHelper : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION()
    static TArray<FPropertyTextFName> GetCatFormatOptions();
};