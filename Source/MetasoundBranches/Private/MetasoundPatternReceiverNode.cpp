#include "MetasoundBranches/Public/MetasoundPatternReceiverNode.h"
#include "MetasoundBranches/Public/MetasoundPatternStream.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_PatternReceiverNode"

namespace Metasound
{
    namespace PatternReceiverNodeVertexNames
    {
        METASOUND_PARAM(InputPatternStream, "Pattern Stream", "The input stream containing generated events.");
        METASOUND_PARAM(InputActive, "Active", "Enable receiving.");
        METASOUND_PARAM(OutputTrigger, "On Receive", "Trigger output when a new event is detected.");
        METASOUND_PARAM(OutputRandomFloat, "Random Float", "The random float from the event.");
        METASOUND_PARAM(OutputPatternStream, "Pattern Stream", "Pass-through stream output.");
    }

    class FPatternReceiverOperator : public TExecutableOperator<FPatternReceiverOperator>
    {
    public:
        FPatternReceiverOperator(const FOperatorSettings& InSettings,
                                 const FPatternStreamReadRef& InPatternStream,
                                 const TDataReadReference<bool>& InActive)
            : PatternStream(InPatternStream)
            , bActive(InActive)
            , OnReceiveTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , OutRandomFloat(FFloatWriteRef::CreateNew(0.0f))
            , OutPatternStream(InPatternStream)
            , LastProcessedIndex(0)
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace PatternReceiverNodeVertexNames;
            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<MetasoundPattern::FPatternStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPatternStream)),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputActive), true)
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger)),
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputRandomFloat)),
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
                Metadata.ClassName = { TEXT("Branches"), TEXT("PatternReceiver"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = LOCTEXT("PatternReceiverDisplayName", "Pattern Receiver");
                Metadata.Description = LOCTEXT("PatternReceiverDesc", "Receives a pattern stream and reproduces the trigger and random float.");
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
            using namespace PatternReceiverNodeVertexNames;
            FDataReferenceCollection Inputs;
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputPatternStream), PatternStream);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputActive), bActive);
            return Inputs;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace PatternReceiverNodeVertexNames;
            FDataReferenceCollection Outputs;
            Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTrigger), OnReceiveTrigger);
            Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputRandomFloat), OutRandomFloat);
            Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputPatternStream), OutPatternStream);
            return Outputs;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace PatternReceiverNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;
            FPatternStreamReadRef PatternStreamRef = InputData.GetOrCreateDefaultDataReadReference<MetasoundPattern::FPatternStream>(
                METASOUND_GET_PARAM_NAME(InputPatternStream), InParams.OperatorSettings
            );
            TDataReadReference<bool> ActiveRef = InputData.GetOrCreateDefaultDataReadReference<bool>(
                METASOUND_GET_PARAM_NAME(InputActive), InParams.OperatorSettings
            );
            return MakeUnique<FPatternReceiverOperator>(InParams.OperatorSettings, PatternStreamRef, ActiveRef);
        }

        void Execute()
        {
            OnReceiveTrigger->AdvanceBlock();
            if (!(*bActive))
            {
                return;
            }
            const TArray<MetasoundPattern::FPatternEvent>& Events = PatternStream->GetEventsInBlock();
            for (int32 i = LastProcessedIndex; i < Events.Num(); ++i)
            {
                OnReceiveTrigger->TriggerFrame(Events[i].BlockSampleFrameIndex);
                *OutRandomFloat = Events[i].ControlValue;
            }
            LastProcessedIndex = Events.Num();
        }

    private:
        FPatternStreamReadRef PatternStream;
        TDataReadReference<bool> bActive;
        FTriggerWriteRef OnReceiveTrigger;
        TDataWriteReference<float> OutRandomFloat;
        FPatternStreamReadRef OutPatternStream;
        int32 LastProcessedIndex;
    };

    class FPatternReceiverNode : public FNodeFacade
    {
    public:
        FPatternReceiverNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FPatternReceiverOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FPatternReceiverNode);
}

#undef LOCTEXT_NAMESPACE