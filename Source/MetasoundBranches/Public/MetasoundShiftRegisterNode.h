#pragma once
#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "MetasoundNode.h"
#include "MetasoundShiftRegisterNode.generated.h"

USTRUCT()
struct FMetaSoundShiftRegisterNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundShiftRegisterNodeConfiguration() : NumStages(8) {}

	/** Number of stages (1-32). */
	UPROPERTY(EditAnywhere, Category="General", meta=(ClampMin="1", ClampMax="32"))
	int32 NumStages;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface>
	OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData>
	GetOperatorData() const override;
};