// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundStereoGainNode.h"
#include "MetasoundExecutableOperator.h"     // TExecutableOperator class
#include "MetasoundPrimitives.h"             // ReadRef and WriteRef descriptions for bool, int32, float, and string
#include "MetasoundNodeRegistrationMacro.h"  // METASOUND_LOCTEXT and METASOUND_REGISTER_NODE macros
#include "MetasoundFacade.h"                 // FNodeFacade class
#include "MetasoundParamHelper.h"            // METASOUND_PARAM and METASOUND_GET_PARAM family of macros

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_StereoGain"

namespace Metasound
{
    namespace StereoGainNodeVertexNames
    {
        METASOUND_PARAM(InputLeftSignal, "In L", "Left channel audio input.");
        METASOUND_PARAM(InputRightSignal, "In R", "Right channel audio input.");
        METASOUND_PARAM(InputGain, "Gain (Lin)", "Gain control (0.0 to 1.0).");

        METASOUND_PARAM(OutputLeftSignal, "Out L", "Left output channel.");
        METASOUND_PARAM(OutputRightSignal, "Out R", "Right output channel.");
    }

    class FStereoGainOperator : public TExecutableOperator<FStereoGainOperator>
    {
    public:
        FStereoGainOperator(
            const FOperatorSettings& InSettings,
            const FAudioBufferReadRef& InLeftSignal,
            const FAudioBufferReadRef& InRightSignal,
            const FFloatReadRef& InGain)
            : InputLeftSignal(InLeftSignal)
            , InputRightSignal(InRightSignal)
            , InputGain(InGain)
            , OutputLeftSignal(FAudioBufferWriteRef::CreateNew(InSettings))
            , OutputRightSignal(FAudioBufferWriteRef::CreateNew(InSettings))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace StereoGainNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLeftSignal)),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRightSignal)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputGain), 1.0f)
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
                Metadata.ClassName = { TEXT("UE"), TEXT("Stereo Gain"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("StereoGainNodeDisplayName", "Stereo Gain");
                Metadata.Description = METASOUND_LOCTEXT("StereoGainNodeDesc", "Scale a stereo input to a gain value.");
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
            using namespace StereoGainNodeVertexNames;

            FDataReferenceCollection InputDataReferences;
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputLeftSignal), InputLeftSignal);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputRightSignal), InputRightSignal);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputGain), InputGain);

            return InputDataReferences;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace StereoGainNodeVertexNames;

            FDataReferenceCollection OutputDataReferences;
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputLeftSignal), OutputLeftSignal);
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputRightSignal), OutputRightSignal);
            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace StereoGainNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            const Metasound::FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<FAudioBuffer> InputLeftSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputLeftSignal), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InputRightSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputRightSignal), InParams.OperatorSettings);
            TDataReadReference<float> Gain = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputGain), InParams.OperatorSettings);

            return MakeUnique<FStereoGainOperator>(InParams.OperatorSettings, InputLeftSignal, InputRightSignal, Gain);
        }

        void Execute()
        {
            const int32 NumFrames = InputLeftSignal->Num();

            const float* LeftData = InputLeftSignal->GetData();
            const float* RightData = InputRightSignal->GetData();
            float* OutputLeftData = OutputLeftSignal->GetData();
            float* OutputRightData = OutputRightSignal->GetData();

            const float GainVal = *InputGain;

            for (int32 i = 0; i < NumFrames; ++i)
            {
                OutputLeftData[i] = LeftData[i] * GainVal;
                OutputRightData[i] = RightData[i] * GainVal;
            }
        }

    private:
        // Inputs
        FAudioBufferReadRef InputLeftSignal;
        FAudioBufferReadRef InputRightSignal;
        FFloatReadRef InputGain;

        // Outputs
        FAudioBufferWriteRef OutputLeftSignal;
        FAudioBufferWriteRef OutputRightSignal;
    };

    class FStereoGainNode : public FNodeFacade
    {
    public:
        FStereoGainNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FStereoGainOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FStereoGainNode);
}

#undef LOCTEXT_NAMESPACE