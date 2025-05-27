// Copyright 2025 Charles Matthews.  All Rights Reserved.
#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "DSP/Filter.h"

#include "MetasoundLadderArCatNode.generated.h"


USTRUCT()
struct FMetaSoundLadderArCatNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()
};