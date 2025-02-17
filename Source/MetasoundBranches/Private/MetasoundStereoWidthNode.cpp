// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundStereoWidthNode.h"
#include "MetasoundExecutableOperator.h"     // TExecutableOperator class
#include "MetasoundPrimitives.h"             // ReadRef and WriteRef descriptions for bool, int32, float, and string
#include "MetasoundNodeRegistrationMacro.h"  // METASOUND_LOCTEXT and METASOUND_REGISTER_NODE macros
#include "MetasoundFacade.h"                 // FNodeFacade class, eliminates the need for a fair amount of boilerplate code
#include "MetasoundParamHelper.h"            // METASOUND_PARAM and METASOUND_GET_PARAM family of macros

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_StereoWidthNode"

namespace Metasound
{
    namespace WidthNodeVertexNames
    {
        METASOUND_PARAM(InputLeftSignal, "In L", "Left channel.");
        METASOUND_PARAM(InputRightSignal, "In R", "Right channel.");
        METASOUND_PARAM(InputWidth, "Width", "Stereo width factor ranging from 0 to 200% (0 - 2).");

        METASOUND_PARAM(OutputLeftSignal, "Out L", "Left channel of the adjusted stereo output signal.");
        METASOUND_PARAM(OutputRightSignal, "Out R", "Right channel of the adjusted stereo output signal.");
    }

    class FWidthOperator : public TExecutableOperator<FWidthOperator>
    {
    public:
        FWidthOperator(
            const FOperatorSettings& InSettings,
            const FAudioBufferReadRef& InLeftSignal,
            const FAudioBufferReadRef& InRightSignal,
            const FFloatReadRef& InWidth)
            : InputLeftSignal(InLeftSignal)
            , InputRightSignal(InRightSignal)
            , InputWidth(InWidth)
            , OutputLeftSignal(FAudioBufferWriteRef::CreateNew(InSettings))
            , OutputRightSignal(FAudioBufferWriteRef::CreateNew(InSettings))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace WidthNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLeftSignal)),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRightSignal)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputWidth), 1.0f)
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

                    Metadata.ClassName = { TEXT("UE"), TEXT("Stereo Width"), TEXT("Audio") };
                    Metadata.MajorVersion = 1;
                    Metadata.MinorVersion = 0;
                    Metadata.DisplayName = METASOUND_LOCTEXT("WidthNodeDisplayName", "Stereo Width");
                    Metadata.Description = METASOUND_LOCTEXT("WidthNodeDesc", "Adjusts the stereo width of a signal.");
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
            using namespace WidthNodeVertexNames;

            FDataReferenceCollection InputDataReferences;

            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputLeftSignal), InputLeftSignal);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputRightSignal), InputRightSignal);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputWidth), InputWidth);

            return InputDataReferences;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace WidthNodeVertexNames;

            FDataReferenceCollection OutputDataReferences;

            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputLeftSignal), OutputLeftSignal);
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputRightSignal), OutputRightSignal);

            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace WidthNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            const Metasound::FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<FAudioBuffer> InputLeftSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputLeftSignal), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InputRightSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputRightSignal), InParams.OperatorSettings);
            TDataReadReference<float> InputWidth = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputWidth), InParams.OperatorSettings);

            return MakeUnique<FWidthOperator>(InParams.OperatorSettings, InputLeftSignal, InputRightSignal, InputWidth);
        }

        void Execute()
        {
            int32 NumFrames = InputLeftSignal->Num();

            const float* LeftData = InputLeftSignal->GetData();
            const float* RightData = InputRightSignal->GetData();
            float* OutputLeftData = OutputLeftSignal->GetData();
            float* OutputRightData = OutputRightSignal->GetData();

            float WidthFactor = FMath::Clamp(*InputWidth, 0.0f, 2.0f);

            for (int32 i = 0; i < NumFrames; ++i)
            {
                float Left = LeftData[i];
                float Right = RightData[i];

                float Mid = 0.5f * (Left + Right);
                float Side = 0.5f * (Left - Right);

                Side *= WidthFactor;

                OutputLeftData[i] = Mid + Side;
                OutputRightData[i] = Mid - Side;
            }
        }

    private:

        // Inputs
        FAudioBufferReadRef InputLeftSignal;
        FAudioBufferReadRef InputRightSignal;
        FFloatReadRef InputWidth;

        // Outputs
        FAudioBufferWriteRef OutputLeftSignal;
        FAudioBufferWriteRef OutputRightSignal;
    };

    class FWidthNode : public FNodeFacade
    {
    public:
        FWidthNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FWidthOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FWidthNode);
}

#undef LOCTEXT_NAMESPACE