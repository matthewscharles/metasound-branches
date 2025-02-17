// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundCrossfadeStereoNode.h"
#include "MetasoundExecutableOperator.h"     // TExecutableOperator class
#include "MetasoundPrimitives.h"             // ReadRef and WriteRef descriptions for bool, int32, float, and string
#include "MetasoundNodeRegistrationMacro.h"  // METASOUND_LOCTEXT and METASOUND_REGISTER_NODE macros
#include "MetasoundFacade.h"                 // FNodeFacade class, eliminates the need for a fair amount of boilerplate code
#include "MetasoundParamHelper.h"            // METASOUND_PARAM and METASOUND_GET_PARAM family of macros

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_CrossfadeStereoNode"

namespace Metasound
{
    namespace CrossfadeStereoNodeVertexNames
    {
        METASOUND_PARAM(InputLeftSignal1, "In1 L", "Left channel of first input.");
        METASOUND_PARAM(InputRightSignal1, "In1 R", "Right channel of first input.");
        METASOUND_PARAM(InputLeftSignal2, "In2 L", "Left channel of second input.");
        METASOUND_PARAM(InputRightSignal2, "In2 R", "Right channel of second input.");
        METASOUND_PARAM(InputCrossfade, "Crossfade", "Crossfade between the two inputs (0.0 to 1.0).");

        METASOUND_PARAM(OutputLeftSignal, "Out L", "Left channel of the output signal.");
        METASOUND_PARAM(OutputRightSignal, "Out R", "Right channel of the output signal.");
    }

    class FCrossfadeStereoOperator : public TExecutableOperator<FCrossfadeStereoOperator>
    {
    public:
        FCrossfadeStereoOperator(
            const FOperatorSettings& InSettings,
            const FAudioBufferReadRef& InLeftSignal1,
            const FAudioBufferReadRef& InRightSignal1,
            const FAudioBufferReadRef& InLeftSignal2,
            const FAudioBufferReadRef& InRightSignal2,
            const FFloatReadRef& InCrossfade)
            : InputLeftSignal1(InLeftSignal1)
            , InputRightSignal1(InRightSignal1)
            , InputLeftSignal2(InLeftSignal2)
            , InputRightSignal2(InRightSignal2)
            , InputCrossfade(InCrossfade)
            , OutputLeftSignal(FAudioBufferWriteRef::CreateNew(InSettings))
            , OutputRightSignal(FAudioBufferWriteRef::CreateNew(InSettings))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace CrossfadeStereoNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLeftSignal1)),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRightSignal1)),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLeftSignal2)),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRightSignal2)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputCrossfade), 0.5f)
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

                    Metadata.ClassName = { TEXT("UE"), TEXT("Stereo Crossfade"), TEXT("Audio") };
                    Metadata.MajorVersion = 1;
                    Metadata.MinorVersion = 1;
                    Metadata.DisplayName = METASOUND_LOCTEXT("CrossfadeStereoNodeDisplayName", "Stereo Crossfade");
                    Metadata.Description = METASOUND_LOCTEXT("CrossfadeStereoNodeDesc", "Crossfades between two stereo signals.");
                    Metadata.Author = "Charles Matthews";
                    Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                    Metadata.DefaultInterface = DeclareVertexInterface();
                    Metadata.CategoryHierarchy = { METASOUND_LOCTEXT("Custom", "Branches") };
                    Metadata.Keywords = TArray<FText>(); // Keywords for searching

                    return Metadata;
                };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            using namespace CrossfadeStereoNodeVertexNames;

            FDataReferenceCollection InputDataReferences;

            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputLeftSignal1), InputLeftSignal1);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputRightSignal1), InputRightSignal1);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputLeftSignal2), InputLeftSignal2);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputRightSignal2), InputRightSignal2);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputCrossfade), InputCrossfade);

            return InputDataReferences;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace CrossfadeStereoNodeVertexNames;

            FDataReferenceCollection OutputDataReferences;

            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputLeftSignal), OutputLeftSignal);
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputRightSignal), OutputRightSignal);

            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace CrossfadeStereoNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            const Metasound::FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<FAudioBuffer> InputLeftSignal1 = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputLeftSignal1), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InputRightSignal1 = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputRightSignal1), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InputLeftSignal2 = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputLeftSignal2), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InputRightSignal2 = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputRightSignal2), InParams.OperatorSettings);
            TDataReadReference<float> InputCrossfade = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputCrossfade), InParams.OperatorSettings);

            return MakeUnique<FCrossfadeStereoOperator>(InParams.OperatorSettings, InputLeftSignal1, InputRightSignal1, InputLeftSignal2, InputRightSignal2, InputCrossfade);
        }

        void Execute()
        {
            int32 NumFrames = InputLeftSignal1->Num();

            const float* LeftData1 = InputLeftSignal1->GetData();
            const float* RightData1 = InputRightSignal1->GetData();
            const float* LeftData2 = InputLeftSignal2->GetData();
            const float* RightData2 = InputRightSignal2->GetData();

            float* OutputLeftData = OutputLeftSignal->GetData();
            float* OutputRightData = OutputRightSignal->GetData();

            float CrossfadeFactor = FMath::Clamp(*InputCrossfade, 0.0f, 1.0f);

            float Gain1 = FMath::Cos(CrossfadeFactor * HALF_PI); // HALF_PI = PI / 2
            float Gain2 = FMath::Sin(CrossfadeFactor * HALF_PI);

            for (int32 i = 0; i < NumFrames; ++i)
            {
                OutputLeftData[i] = Gain1 * LeftData1[i] + Gain2 * LeftData2[i];
                OutputRightData[i] = Gain1 * RightData1[i] + Gain2 * RightData2[i];
            }
        }

    private:

        // Inputs
        FAudioBufferReadRef InputLeftSignal1;
        FAudioBufferReadRef InputRightSignal1;
        FAudioBufferReadRef InputLeftSignal2;
        FAudioBufferReadRef InputRightSignal2;
        FFloatReadRef InputCrossfade;

        // Outputs
        FAudioBufferWriteRef OutputLeftSignal;
        FAudioBufferWriteRef OutputRightSignal;
    };

    class FCrossfadeStereoNode : public FNodeFacade
    {
    public:
        FCrossfadeStereoNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FCrossfadeStereoOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FCrossfadeStereoNode);
}

#undef LOCTEXT_NAMESPACE