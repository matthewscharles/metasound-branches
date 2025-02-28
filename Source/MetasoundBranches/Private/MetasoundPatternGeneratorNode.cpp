#include "MetasoundBranches/Public/MetasoundPatternGeneratorNode.h"
// #include "MetasoundBranches/Public/MetasoundPatternStream.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
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
    }

    class FPatternGeneratorOperator : public TExecutableOperator<FPatternGeneratorOperator>
    {
    public:
        FPatternGeneratorOperator(const FOperatorSettings& InSettings,
                                  const TDataReadReference<float>& InInterval,
                                  const TDataReadReference<bool>& InActive)
            : Interval(InInterval)
            , bActive(InActive)
            , OnGenerateTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , OutRandomFloat(FFloatWriteRef::CreateNew(0.0f))
            , ElapsedTime(0.0f)
            , SampleRate(InSettings.GetSampleRate())
            , NumFrames(InSettings.GetNumFramesPerBlock())
        {
            RandomStream.Initialize(FPlatformTime::Cycles());
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace PatternGeneratorNodeVertexNames;
            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputInterval), 1.0f),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputActive), true)
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
            return Outputs;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace PatternGeneratorNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;
            TDataReadReference<float> IntervalRef = InputData.GetOrCreateDefaultDataReadReference<float>(
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
            const float BlockDurationSec = static_cast<float>(NumFrames) / SampleRate;
            float IntervalSec = FMath::Max(0.001f, *Interval);
            ElapsedTime += BlockDurationSec;
            while (ElapsedTime >= IntervalSec)
            {
                float NewRandom = RandomStream.GetFraction();
                OnGenerateTrigger->TriggerFrame(0);
                *OutRandomFloat = NewRandom;
                ElapsedTime -= IntervalSec;
            }
        }

    private:
        TDataReadReference<float> Interval;
        TDataReadReference<bool>  bActive;
        FTriggerWriteRef OnGenerateTrigger;
        TDataWriteReference<float> OutRandomFloat;
        float ElapsedTime;
        float SampleRate;
        float NumFrames;
        FRandomStream RandomStream;
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