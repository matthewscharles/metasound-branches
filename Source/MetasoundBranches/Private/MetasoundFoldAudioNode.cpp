// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundFoldAudioNode.h"
#include "MetasoundBranches/Public/Fold.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundFoldAudioNode"

namespace Metasound
{
    namespace FoldAudioNodeVertexNames
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
            using namespace FoldAudioNodeVertexNames;

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
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Shapers")
                };
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }
        
        METASOUND_DISABLE_LEGACY_IO()
        
        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace FoldAudioNodeVertexNames;

            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal), InputSignal);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputHigh), InputHigh);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputLow), InputLow);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputHighMod), InputHighMod);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputLowMod), InputLowMod);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace FoldAudioNodeVertexNames;

            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), OutputSignal);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace FoldAudioNodeVertexNames;

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
                float LowValue  = *InputLow  + LowModData[i];

                float FoldedSample = PerformFold(SignalData[i], LowValue, HighValue);
                OutputDataPtr[i] = FoldedSample;
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

    class FFoldAudioNode : public FNodeFacade
    {
    public:
        FFoldAudioNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FFoldOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FFoldAudioNode);
}

#undef LOCTEXT_NAMESPACE
