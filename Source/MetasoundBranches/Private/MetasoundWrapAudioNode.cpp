// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundWrapAudioNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "MetasoundWrapNode"

namespace Metasound
{
    
    inline float PerformWrap(float InSample, float Low, float High)
    {
        // Ensure Low <= High
        float NewLow  = FMath::Min(Low, High);
        float NewHigh = FMath::Max(Low, High);

        float Range = NewHigh - NewLow;
        if (Range <= 0.0f)
        {
            // Invalid or zero range, do nothing
            return InSample;
        }

        // Shift input so that 'NewLow' is zero
        float Shifted = InSample - NewLow;

        // Wrap into [0, Range)
        float Wrapped = fmodf(Shifted, Range);

        // fmodf can yield negative values if Shifted < 0
        if (Wrapped < 0.0f)
        {
            Wrapped += Range;
        }

        // Shift back into [NewLow, NewHigh]
        return Wrapped + NewLow;
    }

    namespace WrapNodeVertexNames
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
            using namespace WrapNodeVertexNames;

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
                Metadata.CategoryHierarchy = { METASOUND_LOCTEXT("Custom", "Branches") };
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            using namespace WrapNodeVertexNames;

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
            using namespace WrapNodeVertexNames;

            FDataReferenceCollection OutputDataReferences;
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputSignal), OutputSignal);
            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace WrapNodeVertexNames;

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

    class FWrapNode : public FNodeFacade
    {
    public:
        FWrapNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FWrapOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FWrapNode);
}

#undef LOCTEXT_NAMESPACE