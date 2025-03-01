// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundPatternGeneratorNode.h"
#include "MetasoundBranches/Public/MetasoundPatternStream.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_PatternGeneratorNode"

namespace Metasound
{
    namespace PatternGeneratorNodeVertexNames
    {
        METASOUND_PARAM(InputPeriod,          "Period",          "Base time duration for triggers.");
        METASOUND_PARAM(InputTimeMultipliers, "Time Multipliers","Array of multipliers for the base period.");
        METASOUND_PARAM(InputActive,          "Active",          "Enable generation.");
        METASOUND_PARAM(OutputTrigger,        "On Generate",     "Trigger output when a new event is generated.");
        METASOUND_PARAM(OutputCurrentIndex,   "Current Index",   "The current index in the time multipliers array.");
        METASOUND_PARAM(OutputPatternStream,  "Pattern Stream",  "Stream output containing the generated events.");
    }

    class FPatternGeneratorOperator : public TExecutableOperator<FPatternGeneratorOperator>
    {
    public:
        FPatternGeneratorOperator(const FOperatorSettings& InSettings,
                                  const FTimeReadRef& InPeriod,
                                  const TDataReadReference<TArray<float>>& InTimeMultipliers,
                                  const TDataReadReference<bool>& InActive)
            : InputPeriod(InPeriod)
            , InputTimeMultipliers(InTimeMultipliers)
            , bActive(InActive)
            , OnGenerateTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , OutCurrentIndex(FInt32WriteRef::CreateNew(0))
            , OutPatternStream(FPatternStreamWriteRef::CreateNew(MetasoundPattern::FPatternStream()))
            , SampleRate(InSettings.GetSampleRate())
            , NumFrames(InSettings.GetNumFramesPerBlock())
            , SampleCounter(0, SampleRate)
            , CurrentIndex(0)
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace PatternGeneratorNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPeriod)),
                    TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTimeMultipliers)),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputActive))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger)),
                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputCurrentIndex)),
                    TOutputDataVertex<MetasoundPattern::FPatternStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputPatternStream))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("Branches"), TEXT("PatternGenerator"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = LOCTEXT("PatternGeneratorDisplayName", "Pattern Generator");
                Metadata.Description = LOCTEXT("PatternGeneratorDesc", "Generates triggers based on an array of multipliers applied to a base period.");
                Metadata.Author = TEXT("Charles Matthews");
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = { LOCTEXT("CustomCategory", "Branches") };
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            using namespace PatternGeneratorNodeVertexNames;

            FDataReferenceCollection Inputs;
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputPeriod),         InputPeriod);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTimeMultipliers),InputTimeMultipliers);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputActive),         bActive);
            return Inputs;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace PatternGeneratorNodeVertexNames;

            FDataReferenceCollection Outputs;
            Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTrigger),       OnGenerateTrigger);
            Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputCurrentIndex), OutCurrentIndex);
            Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputPatternStream),OutPatternStream);
            return Outputs;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace PatternGeneratorNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            FTimeReadRef PeriodRef = InputData.GetOrCreateDefaultDataReadReference<FTime>(
                METASOUND_GET_PARAM_NAME(InputPeriod), InParams.OperatorSettings
            );

            TDataReadReference<TArray<float>> TimeMultipliersRef = InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(
                METASOUND_GET_PARAM_NAME(InputTimeMultipliers), InParams.OperatorSettings
            );

            TDataReadReference<bool> ActiveRef = InputData.GetOrCreateDefaultDataReadReference<bool>(
                METASOUND_GET_PARAM_NAME(InputActive), InParams.OperatorSettings
            );

            return MakeUnique<FPatternGeneratorOperator>(InParams.OperatorSettings, PeriodRef, TimeMultipliersRef, ActiveRef);
        }

        void Execute()
        {
            OnGenerateTrigger->AdvanceBlock();

            if (!(*bActive) || InputTimeMultipliers->Num() == 0)
            {
                return;
            }

            float CurrentMultiplier = (*InputTimeMultipliers)[CurrentIndex % InputTimeMultipliers->Num()];
            FSampleCount IntervalInSamples = FSampleCounter::FromTime((*InputPeriod) * CurrentMultiplier, SampleRate).GetNumSamples();
            IntervalInSamples = FMath::Max(static_cast<FSampleCount>(1), IntervalInSamples);

            const int32 NumFramesInt = static_cast<int32>(NumFrames);

            while ((SampleCounter - NumFramesInt).GetNumSamples() <= 0)
            {
                int32 TriggerFrame = static_cast<int32>(SampleCounter.GetNumSamples());
                TriggerFrame = FMath::Clamp(TriggerFrame, 0, NumFramesInt - 1);

                OnGenerateTrigger->TriggerFrame(TriggerFrame);
                *OutCurrentIndex = CurrentIndex;

                MetasoundPattern::FPatternEvent NewEvent;
                NewEvent.BlockSampleFrameIndex = TriggerFrame;
                NewEvent.ControlValue = CurrentMultiplier;
                OutPatternStream->AddEvent(NewEvent);

                SampleCounter += IntervalInSamples;
                CurrentIndex = (CurrentIndex + 1) % InputTimeMultipliers->Num();
            }

            SampleCounter -= NumFramesInt;
        }

    private:
        FTimeReadRef InputPeriod;
        TDataReadReference<TArray<float>> InputTimeMultipliers;
        TDataReadReference<bool> bActive;
        FTriggerWriteRef OnGenerateTrigger;
        TDataWriteReference<int32> OutCurrentIndex;
        FPatternStreamWriteRef OutPatternStream;
        float SampleRate;
        float NumFrames;
        FSampleCounter SampleCounter;
        int32 CurrentIndex;
    };

    class FPatternGeneratorNode : public FNodeFacade
    {
    public:
        FPatternGeneratorNode(const FNodeInitData& InitData)
            : FNodeFacade(
                InitData.InstanceName,
                InitData.InstanceID,
                TFacadeOperatorClass<FPatternGeneratorOperator>()
              )
        {
        }
    };

    METASOUND_REGISTER_NODE(FPatternGeneratorNode);
}

#undef LOCTEXT_NAMESPACE