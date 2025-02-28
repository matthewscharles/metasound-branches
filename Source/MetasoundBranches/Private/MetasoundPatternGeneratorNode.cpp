#include "MetasoundBranches/Public/MetasoundPatternGeneratorNode.h"
// #include "MetasoundBranches/Public/MetasoundPatternStream.h"
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
        METASOUND_PARAM(InputInterval, "Interval", "Time in seconds between each random value generation.");
        METASOUND_PARAM(InputActive, "Active", "Enable generation.");
        METASOUND_PARAM(OutputTrigger, "On Generate", "Trigger output when a new random value is generated.");
        METASOUND_PARAM(OutputRandomFloat, "Random Float", "The newly generated random float.");
        // METASOUND_PARAM(OutputPatternStream, "Pattern Stream", "Stream output containing the generated events."); // *
    }

    class FPatternGeneratorOperator : public TExecutableOperator<FPatternGeneratorOperator>
    {
    public:
        FPatternGeneratorOperator(const FOperatorSettings& InSettings,
                                  const FTimeReadRef& InInterval,
                                  const TDataReadReference<bool>& InActive)
            : Interval(InInterval)
            , bActive(InActive)
            , OnGenerateTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , OutRandomFloat(FFloatWriteRef::CreateNew(0.0f))
             // , OutPatternStream(FPatternStreamWriteRef::CreateNew(MetasoundPattern::FPatternStream())) // *
            , SampleRate(InSettings.GetSampleRate())
            , NumFrames(InSettings.GetNumFramesPerBlock())
            , SampleCounter(0, SampleRate)
        {
            RandomStream.Initialize(FPlatformTime::Cycles());
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace PatternGeneratorNodeVertexNames;
            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputInterval), 1.0f),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputActive), true)
                    // , TOutputDataVertex<MetasoundPattern::FPatternStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputPatternStream)) // *
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger)),
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputRandomFloat))
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
                Metadata.Description = LOCTEXT("PatternGeneratorDesc", "Generates a random float at a specified interval.");
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
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputInterval), Interval);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputActive), bActive);
            return Inputs;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace PatternGeneratorNodeVertexNames;
            FDataReferenceCollection Outputs;
            Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTrigger), OnGenerateTrigger);
            Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputRandomFloat), OutRandomFloat);
            // , TOutputDataVertex<MetasoundPattern::FPatternStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputPatternStream)) // *
            return Outputs;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace PatternGeneratorNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;
            FTimeReadRef IntervalRef = InputData.GetOrCreateDefaultDataReadReference<FTime>(
                METASOUND_GET_PARAM_NAME(InputInterval), InParams.OperatorSettings
            );
            TDataReadReference<bool> ActiveRef = InputData.GetOrCreateDefaultDataReadReference<bool>(
                METASOUND_GET_PARAM_NAME(InputActive), InParams.OperatorSettings
            );
            return MakeUnique<FPatternGeneratorOperator>(InParams.OperatorSettings, IntervalRef, ActiveRef);
        }

        void Execute()
        {
            OnGenerateTrigger->AdvanceBlock();

            if (!(*bActive))
            {
                return;
            }

            FSampleCount IntervalInSamples = FSampleCounter::FromTime(*Interval, SampleRate).GetNumSamples();
            IntervalInSamples = FMath::Max(static_cast<FSampleCount>(1), IntervalInSamples);

            const int32 NumFramesInt = static_cast<int32>(NumFrames);

            while ((SampleCounter - NumFramesInt).GetNumSamples() <= 0)
            {
                OnGenerateTrigger->TriggerFrame(static_cast<int32>(SampleCounter.GetNumSamples()));
                float NewRandom = RandomStream.GetFraction();
                *OutRandomFloat = NewRandom;

                SampleCounter += IntervalInSamples;
            }

            SampleCounter -= NumFramesInt;
                // MetasoundPattern::FPatternEvent NewEvent; // *
                // NewEvent.BlockSampleFrameIndex = 0;
                // NewEvent.ControlValue = NewRandom;
                // OutPatternStream->AddEvent(NewEvent);
        }

    private:
        FTimeReadRef Interval;
        TDataReadReference<bool> bActive;
        FTriggerWriteRef OnGenerateTrigger;
        TDataWriteReference<float> OutRandomFloat;
        // FPatternStreamWriteRef     OutPatternStream; // *
        float SampleRate;
        float NumFrames;
        FRandomStream RandomStream;
        FSampleCounter SampleCounter;
    };

    class FPatternGeneratorNode : public FNodeFacade
    {
    public:
        FPatternGeneratorNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FPatternGeneratorOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FPatternGeneratorNode);
}

#undef LOCTEXT_NAMESPACE