// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundStereoInverterNode.h"
#include "MetasoundExecutableOperator.h"     // TExecutableOperator class
#include "MetasoundPrimitives.h"             // ReadRef and WriteRef descriptions for bool, int32, float, and string
#include "MetasoundNodeRegistrationMacro.h"  // METASOUND_LOCTEXT and METASOUND_REGISTER_NODE macros
#include "MetasoundFacade.h"                 // FNodeFacade class
#include "MetasoundParamHelper.h"            // METASOUND_PARAM and METASOUND_GET_PARAM family of macros

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_StereoInverter"

namespace Metasound
{
    namespace StereoInverterNodeVertexNames
    {
        METASOUND_PARAM(InputLeftSignal, "In L", "Left channel audio output.");
        METASOUND_PARAM(InputRightSignal, "In R", "Right channel audio output.");
        METASOUND_PARAM(InputInvertLeft, "Invert L", "Invert the polarity of the left channel.");
        METASOUND_PARAM(InputInvertRight, "Invert R", "Invert the polarity of the right channel.");
        METASOUND_PARAM(InputSwapChannels, "Swap L/R", "Swap the left and right channels.");

        METASOUND_PARAM(OutputLeftSignal, "Out L", "Left output channel.");
        METASOUND_PARAM(OutputRightSignal, "Out R", "Right output channel.");
    }

    class FStereoInverterOperator : public TExecutableOperator<FStereoInverterOperator>
    {
    public:
        FStereoInverterOperator(
            const FOperatorSettings& InSettings,
            const FAudioBufferReadRef& InLeftSignal,
            const FAudioBufferReadRef& InRightSignal,
            const FBoolReadRef& InInvertLeft,
            const FBoolReadRef& InInvertRight,
            const FBoolReadRef& InSwapChannels)
            : InputLeftSignal(InLeftSignal)
            , InputRightSignal(InRightSignal)
            , InputInvertLeft(InInvertLeft)
            , InputInvertRight(InInvertRight)
            , InputSwapChannels(InSwapChannels)
            , OutputLeftSignal(FAudioBufferWriteRef::CreateNew(InSettings))
            , OutputRightSignal(FAudioBufferWriteRef::CreateNew(InSettings))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace StereoInverterNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLeftSignal)),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRightSignal)),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputInvertLeft), false),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputInvertRight), false),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSwapChannels), false)
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
                Metadata.ClassName = { TEXT("UE"), TEXT("Stereo Inverter"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("StereoInverterNodeDisplayName", "Stereo Inverter");
                Metadata.Description = METASOUND_LOCTEXT("StereoInverterNodeDesc", "Inverts and/or swaps stereo channels.");
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
            using namespace StereoInverterNodeVertexNames;

            FDataReferenceCollection InputDataReferences;
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputLeftSignal), InputLeftSignal);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputRightSignal), InputRightSignal);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputInvertLeft), InputInvertLeft);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputInvertRight), InputInvertRight);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputSwapChannels), InputSwapChannels);

            return InputDataReferences;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace StereoInverterNodeVertexNames;

            FDataReferenceCollection OutputDataReferences;
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputLeftSignal), OutputLeftSignal);
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputRightSignal), OutputRightSignal);
            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace StereoInverterNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            const Metasound::FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<FAudioBuffer> InputLeftSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputLeftSignal), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InputRightSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputRightSignal), InParams.OperatorSettings);

            TDataReadReference<bool> InvertLeft = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputInvertLeft), InParams.OperatorSettings);
            TDataReadReference<bool> InvertRight = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputInvertRight), InParams.OperatorSettings);
            TDataReadReference<bool> SwapChannels = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputSwapChannels), InParams.OperatorSettings);

            return MakeUnique<FStereoInverterOperator>(InParams.OperatorSettings, InputLeftSignal, InputRightSignal, InvertLeft, InvertRight, SwapChannels);
        }

        void Execute()
        {
            const int32 NumFrames = InputLeftSignal->Num();

            const float* LeftData = InputLeftSignal->GetData();
            const float* RightData = InputRightSignal->GetData();
            float* OutputLeftData = OutputLeftSignal->GetData();
            float* OutputRightData = OutputRightSignal->GetData();

            const bool bInvertL = *InputInvertLeft;
            const bool bInvertR = *InputInvertRight;
            const bool bSwap = *InputSwapChannels;

            const float LMult = bInvertL ? -1.0f : 1.0f;
            const float RMult = bInvertR ? -1.0f : 1.0f;

            for (int32 i = 0; i < NumFrames; ++i)
            {
                const float LIn = LeftData[i];
                const float RIn = RightData[i];

                if (bSwap)
                {
                    OutputLeftData[i] = RIn * LMult;
                    OutputRightData[i] = LIn * RMult;
                }
                else
                {
                    OutputLeftData[i] = LIn * LMult;
                    OutputRightData[i] = RIn * RMult;
                }
            }
        }

    private:
        // Inputs
        FAudioBufferReadRef InputLeftSignal;
        FAudioBufferReadRef InputRightSignal;
        FBoolReadRef InputInvertLeft;
        FBoolReadRef InputInvertRight;
        FBoolReadRef InputSwapChannels;

        // Outputs
        FAudioBufferWriteRef OutputLeftSignal;
        FAudioBufferWriteRef OutputRightSignal;
    };

    class FStereoInverterNode : public FNodeFacade
    {
    public:
        FStereoInverterNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FStereoInverterOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FStereoInverterNode);
}

#undef LOCTEXT_NAMESPACE