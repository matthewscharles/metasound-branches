// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundWrapAudioNode.h"
#include "MetasoundBranches/Public/Wrap.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundWrapAudioNode"

namespace Metasound
{
    namespace WrapAudioNodeVertexNames
    {
        METASOUND_PARAM(InputSignal,    "In",       "Audio signal to wrap.");
        METASOUND_PARAM(InputHigh,      "High",     "Upper threshold for wrapping.");
        METASOUND_PARAM(InputLow,       "Low",      "Lower threshold for wrapping.");
        METASOUND_PARAM(InputHighMod,   "High Mod", "Modulation for high threshold.");
        METASOUND_PARAM(InputLowMod,    "Low Mod",  "Modulation for low threshold.");
        METASOUND_PARAM(OutputSignal,   "Out",      "Wrapped audio signal.");
    }

    class FWrapOperator : public TExecutableOperator<FWrapOperator>
    {
    public:
        FWrapOperator(
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
            using namespace WrapAudioNodeVertexNames;

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
                Metadata.ClassName = { TEXT("UE"), TEXT("Wrap (Audio)"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("WrapDisplayName", "Wrap (Audio)");
                Metadata.Description = METASOUND_LOCTEXT("WrapDesc", "Wraps the audio signal within a Low/High range.");
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
            using namespace WrapAudioNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal), InputSignal);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputHigh), InputHigh);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputLow), InputLow);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputHighMod), InputHighMod);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputLowMod), InputLowMod);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace WrapAudioNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), OutputSignal);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace WrapAudioNodeVertexNames;

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

            return MakeUnique<FWrapOperator>(InParams.OperatorSettings, InputSignal, InputHigh, InputLow, InputHighMod, InputLowMod);
        }

        virtual void Execute()
        {
            int32 NumFrames = InputSignal->Num();
            const float* SignalData = InputSignal->GetData();
            float* OutputData = OutputSignal->GetData();
            const float* HighModData = InputHighMod->GetData();
            const float* LowModData  = InputLowMod->GetData();

            for (int32 i = 0; i < NumFrames; ++i)
            {
                float HighValue = *InputHigh + HighModData[i];
                float LowValue  = *InputLow  + LowModData[i];

                // Perform a modulo-based wrap for each sample
                float WrappedSample = PerformWrap(SignalData[i], LowValue, HighValue);
                OutputData[i] = WrappedSample;
            }
        }

    private:
        // Inputs
        FAudioBufferReadRef InputSignal;
        FFloatReadRef InputHigh;
        FFloatReadRef InputLow;
        FAudioBufferReadRef InputHighMod;
        FAudioBufferReadRef InputLowMod;

        // Output
        FAudioBufferWriteRef OutputSignal;
    };

    class FWrapAudioNode : public FNodeFacade
    {
    public:
        FWrapAudioNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FWrapOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FWrapAudioNode);
}

#undef LOCTEXT_NAMESPACE