// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundStereoBalanceNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_BalanceNode"

namespace Metasound
{
    namespace BalanceNodeVertexNames
    {
        METASOUND_PARAM(InputLeftSignal, "In L", "Left channel audio input.");
        METASOUND_PARAM(InputRightSignal, "In R", "Right channel audio input.");
        METASOUND_PARAM(InputBalance, "Balance", "Balance control ranging from -1.0 (full left) to 1.0 (full right).");
        METASOUND_PARAM(InputBalanceModulation, "Modulation", "Audio-rate balance modulation signal.");

        METASOUND_PARAM(OutputLeftSignal, "Out L", "Left output channel.");
        METASOUND_PARAM(OutputRightSignal, "Out R", "Right output channel.");
    }

    class FBalanceOperator : public TExecutableOperator<FBalanceOperator>
    {
    public:
        FBalanceOperator(
            const FOperatorSettings& InSettings,
            const FAudioBufferReadRef& InLeftSignal,
            const FAudioBufferReadRef& InRightSignal,
            const FFloatReadRef& InBalance,
            const FAudioBufferReadRef& InBalanceModulation)
            : InputLeftSignal(InLeftSignal)
            , InputRightSignal(InRightSignal)
            , InputBalance(InBalance)
            , InputBalanceModulation(InBalanceModulation)
            , OutputLeftSignal(FAudioBufferWriteRef::CreateNew(InSettings))
            , OutputRightSignal(FAudioBufferWriteRef::CreateNew(InSettings))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace BalanceNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLeftSignal)),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRightSignal)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBalance), 0.0f),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBalanceModulation))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputLeftSignal)),
                    TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputRightSignal))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FVertexInterface NodeInterface = DeclareVertexInterface();

                FNodeClassMetadata Metadata;

                Metadata.ClassName = { TEXT("UE"), TEXT("Stereo Balance"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 2;
                Metadata.DisplayName = METASOUND_LOCTEXT("StereoBalanceNodeDisplayName", "Stereo Balance");
                Metadata.Description = METASOUND_LOCTEXT("StereoBalanceNodeDesc", "Adjusts the balance of a stereo signal with optional AR modulation.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = NodeInterface;
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Spatialization")
                };
                Metadata.Keywords = TArray<FText>();

                METASOUND_BRANCHES_APPLY_NODE_STYLE(Metadata);

                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        METASOUND_DISABLE_LEGACY_IO()

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace BalanceNodeVertexNames;

            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputLeftSignal), InputLeftSignal);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputRightSignal), InputRightSignal);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBalance), InputBalance);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBalanceModulation), InputBalanceModulation);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace BalanceNodeVertexNames;

            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputLeftSignal), OutputLeftSignal);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputRightSignal), OutputRightSignal);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace BalanceNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<FAudioBuffer> InputLeftSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                METASOUND_GET_PARAM_NAME(InputLeftSignal), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InputRightSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                METASOUND_GET_PARAM_NAME(InputRightSignal), InParams.OperatorSettings);
            TDataReadReference<float> InputBalance = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputBalance), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InputBalanceModulation = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                METASOUND_GET_PARAM_NAME(InputBalanceModulation), InParams.OperatorSettings);

            return MakeUnique<FBalanceOperator>(
                InParams.OperatorSettings,
                InputLeftSignal,
                InputRightSignal,
                InputBalance,
                InputBalanceModulation
            );
        }

        void Execute()
        {
            const int32 NumFrames = InputLeftSignal->Num();

            const float* LeftData = InputLeftSignal->GetData();
            const float* RightData = InputRightSignal->GetData();
            const float* ModulationData = InputBalanceModulation->GetData();

            float* OutputLeftData = OutputLeftSignal->GetData();
            float* OutputRightData = OutputRightSignal->GetData();

            const float ScalarBalance = *InputBalance;

            for (int32 i = 0; i < NumFrames; ++i)
            {
                float Balance = FMath::Clamp(ScalarBalance + ModulationData[i], -1.0f, 1.0f);
                float Angle = (Balance + 1.0f) * (PI / 4.0f);

                float LeftGain = FMath::Cos(Angle);
                float RightGain = FMath::Sin(Angle);

                OutputLeftData[i] = LeftData[i] * LeftGain;
                OutputRightData[i] = RightData[i] * RightGain;
            }
        }

    private:
        // Inputs
        FAudioBufferReadRef InputLeftSignal;
        FAudioBufferReadRef InputRightSignal;
        FFloatReadRef InputBalance;
        FAudioBufferReadRef InputBalanceModulation;

        // Outputs
        FAudioBufferWriteRef OutputLeftSignal;
        FAudioBufferWriteRef OutputRightSignal;
    };

    class FBalanceNode : public FNodeFacade
    {
    public:
                static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FBalanceOperator::GetNodeInfo();
        }
        FBalanceNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FBalanceOperator::GetNodeInfo()), TFacadeOperatorClass<FBalanceOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FBalanceNode);
}

#undef LOCTEXT_NAMESPACE