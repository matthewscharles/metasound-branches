// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundFoldAudioNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundFoldNode"

namespace Metasound
{
    namespace FoldNodeVertexNames
    {
        METASOUND_PARAM(InputSignal, "In", "Audio signal to fold.");
        METASOUND_PARAM(InputHigh, "High", "Upper threshold for folding.");
        METASOUND_PARAM(InputLow, "Low", "Lower threshold for folding.");
        METASOUND_PARAM(InputHighMod, "High Mod", "Modulation for high threshold.");
        METASOUND_PARAM(InputLowMod, "Low Mod", "Modulation for low threshold.");
        METASOUND_PARAM(OutputSignal, "Out", "Folded audio signal.");
    }

    class FFoldOperator : public TExecutableOperator<FFoldOperator>
    {
    public:
        FFoldOperator(
            const FOperatorSettings& InSettings,
            const FAudioBufferReadRef& InSignal,
            const FFloatReadRef& InHigh,
            const FFloatReadRef& InLow,
            const FAudioBufferReadRef& InHighMod,
            const FAudioBufferReadRef& InLowMod)
            : InputSignal(InSignal)
            , InputHigh(InHigh)
            , InputLow(InLow)
            , InputHighMod(InHighMod)
            , InputLowMod(InLowMod)
            , OutputSignal(FAudioBufferWriteRef::CreateNew(InSettings))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace FoldNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignal)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputHigh)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLow)),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputHighMod)),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLowMod))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputSignal))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("Fold (Audio)"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 1;
                Metadata.DisplayName = METASOUND_LOCTEXT("FoldDisplayName", "Fold (Audio)");
                Metadata.Description = METASOUND_LOCTEXT("FoldDesc", "Folds the audio signal based on high and low thresholds.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = { METASOUND_LOCTEXT("Custom", "Branches") };
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            using namespace FoldNodeVertexNames;

            FDataReferenceCollection InputDataReferences;
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputSignal), InputSignal);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputHigh), InputHigh);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputLow), InputLow);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputHighMod), InputHighMod);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputLowMod), InputLowMod);

            return InputDataReferences;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace FoldNodeVertexNames;

            FDataReferenceCollection OutputDataReferences;
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputSignal), OutputSignal);
            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace FoldNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<FAudioBuffer> InputSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                METASOUND_GET_PARAM_NAME(InputSignal), InParams.OperatorSettings);

            TDataReadReference<float> InputHigh = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputHigh), InParams.OperatorSettings);

            TDataReadReference<float> InputLow = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputLow), InParams.OperatorSettings);

            TDataReadReference<FAudioBuffer> InputHighMod = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                METASOUND_GET_PARAM_NAME(InputHighMod), InParams.OperatorSettings);

            TDataReadReference<FAudioBuffer> InputLowMod = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                METASOUND_GET_PARAM_NAME(InputLowMod), InParams.OperatorSettings);

            return MakeUnique<FFoldOperator>(InParams.OperatorSettings, InputSignal, InputHigh, InputLow, InputHighMod, InputLowMod);
        }

        virtual void Execute()
        {
            int32 NumFrames = InputSignal->Num();
            const float* SignalData = InputSignal->GetData();
            float* OutputDataPtr = OutputSignal->GetData();
            const float* HighModData = InputHighMod->GetData();
            const float* LowModData = InputLowMod->GetData();

            for (int32 i = 0; i < NumFrames; ++i)
            {
                float HighValue = *InputHigh + HighModData[i];
                float LowValue = *InputLow + LowModData[i];
                float Sample = SignalData[i];
                
                while (Sample > HighValue)
                    Sample = 2.0f * HighValue - Sample;
                while (Sample < LowValue)
                    Sample = 2.0f * LowValue - Sample;

                OutputDataPtr[i] = Sample;
            }
        }

    private:
        FAudioBufferReadRef InputSignal;
        FFloatReadRef InputHigh;
        FFloatReadRef InputLow;
        FAudioBufferReadRef InputHighMod;
        FAudioBufferReadRef InputLowMod;
        
        FAudioBufferWriteRef OutputSignal;
    };

    class FFoldNode : public FNodeFacade
    {
    public:
        FFoldNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FFoldOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FFoldNode);
}

#undef LOCTEXT_NAMESPACE
