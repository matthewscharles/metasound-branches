// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundStereoBalanceNode.h"
#include "MetasoundExecutableOperator.h"     // TExecutableOperator class
#include "MetasoundPrimitives.h"             // ReadRef and WriteRef descriptions for bool, int32, float, and string
#include "MetasoundNodeRegistrationMacro.h"  // METASOUND_LOCTEXT and METASOUND_REGISTER_NODE macros
#include "MetasoundFacade.h"                 // FNodeFacade class, eliminates the need for a fair amount of boilerplate code
#include "MetasoundParamHelper.h"            // METASOUND_PARAM and METASOUND_GET_PARAM family of macros
#include "Math/UnrealMathUtility.h"          // For FMath functions

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_BalanceNode"

namespace Metasound
{
    namespace BalanceNodeVertexNames
    {
        METASOUND_PARAM(InputLeftSignal, "In L", "Left channel audio input.");
        METASOUND_PARAM(InputRightSignal, "In R", "Right channel audio input.");
        METASOUND_PARAM(InputBalance, "Balance", "Balance control ranging from -1.0 (full left) to 1.0 (full right).");

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
            const FFloatReadRef& InBalance)
            : InputLeftSignal(InLeftSignal)
            , InputRightSignal(InRightSignal)
            , InputBalance(InBalance)
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
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBalance), 0.0f) // Default balance is centered
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
                    Metadata.MinorVersion = 1;
                    Metadata.DisplayName = METASOUND_LOCTEXT("StereoGainNodeDisplayName", "Stereo Balance");
                    Metadata.Description = METASOUND_LOCTEXT("StereoGainNodeDesc", "Adjusts the balance of a stereo signal.");
                    Metadata.Author = "Charles Matthews";
                    Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                    Metadata.DefaultInterface = NodeInterface;
                    Metadata.CategoryHierarchy = { METASOUND_LOCTEXT("Custom", "Branches") };
                    Metadata.Keywords = TArray<FText>();

                    return Metadata;
                };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            using namespace BalanceNodeVertexNames;

            FDataReferenceCollection InputDataReferences;

            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputLeftSignal), InputLeftSignal);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputRightSignal), InputRightSignal);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputBalance), InputBalance);

            return InputDataReferences;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace BalanceNodeVertexNames;

            FDataReferenceCollection OutputDataReferences;

            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputLeftSignal), OutputLeftSignal);
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputRightSignal), OutputRightSignal);

            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace BalanceNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            const Metasound::FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<FAudioBuffer> InputLeftSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputLeftSignal), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InputRightSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputRightSignal), InParams.OperatorSettings);
            TDataReadReference<float> InputBalance = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputBalance), InParams.OperatorSettings);

            return MakeUnique<FBalanceOperator>(InParams.OperatorSettings, InputLeftSignal, InputRightSignal, InputBalance);
        }

    void Execute()
    {
        int32 NumFrames = InputLeftSignal->Num();

        const float* LeftData = InputLeftSignal->GetData();
        const float* RightData = InputRightSignal->GetData();
        float* OutputLeftData = OutputLeftSignal->GetData();
        float* OutputRightData = OutputRightSignal->GetData();

        float Balance = FMath::Clamp(*InputBalance, -1.0f, 1.0f);

        float Angle = (Balance + 1.0f) * (PI / 4.0f);

        float LeftGain = FMath::Cos(Angle);
        float RightGain = FMath::Sin(Angle);

        for (int32 i = 0; i < NumFrames; ++i)
        {
            OutputLeftData[i] = LeftData[i] * LeftGain;
            OutputRightData[i] = RightData[i] * RightGain;
        }
    }

    private:

        // Inputs
        FAudioBufferReadRef InputLeftSignal;
        FAudioBufferReadRef InputRightSignal;
        FFloatReadRef InputBalance;

        // Outputs
        FAudioBufferWriteRef OutputLeftSignal;
        FAudioBufferWriteRef OutputRightSignal;
    };

    class FBalanceNode : public FNodeFacade
    {
    public:
        FBalanceNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FBalanceOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FBalanceNode);
}

#undef LOCTEXT_NAMESPACE