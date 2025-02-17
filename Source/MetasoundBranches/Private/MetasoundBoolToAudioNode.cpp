// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundBoolToAudioNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundBoolToAudioNode"

namespace Metasound
{
    namespace BoolToAudioNodeVertexNames
    {
        METASOUND_PARAM(InputBool, "Value", "Boolean input to convert to audio.");
        METASOUND_PARAM(InputRiseTime, "Rise Time", "Rise time in seconds.");
        METASOUND_PARAM(InputFallTime, "Fall Time", "Fall time in seconds.");
        METASOUND_PARAM(OutputSignal, "Out", "Audio signal.");
    }

    class FBoolToAudioOperator : public TExecutableOperator<FBoolToAudioOperator>
    {
    public:
        FBoolToAudioOperator(
            const FOperatorSettings& InSettings,
            const FBoolReadRef& InBool,
            const FTimeReadRef& InRiseTime,
            const FTimeReadRef& InFallTime)
            : InputBool(InBool)
            , InputRiseTime(InRiseTime)
            , InputFallTime(InFallTime)
            , OutputSignal(FAudioBufferWriteRef::CreateNew(InSettings))
            , PreviousOutputSample(0.0f)
            , SampleRate(InSettings.GetSampleRate())
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace BoolToAudioNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBool)),
                    TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRiseTime)),
                    TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFallTime))
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
                Metadata.ClassName = { TEXT("UE"), TEXT("BoolToAudio"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("BoolToAudioDisplayName", "Bool To Audio");
                Metadata.Description = METASOUND_LOCTEXT("BoolToAudioDesc", "Converts a boolean value to an audio signal, with optional rise and fall times.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = { METASOUND_LOCTEXT("Custom", "Branches") };
                Metadata.Keywords = TArray<FText>();

                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            using namespace BoolToAudioNodeVertexNames;

            FDataReferenceCollection InputDataReferences;
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputBool), InputBool);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputRiseTime), InputRiseTime);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputFallTime), InputFallTime);

            return InputDataReferences;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace BoolToAudioNodeVertexNames;

            FDataReferenceCollection OutputDataReferences;
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputSignal), OutputSignal);

            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace BoolToAudioNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            const FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<bool> InputBool = InputData.GetOrCreateDefaultDataReadReference<bool>(
                METASOUND_GET_PARAM_NAME(InputBool),
                InParams.OperatorSettings
            );

            TDataReadReference<FTime> InputRiseTime = InputData.GetOrCreateDefaultDataReadReference<FTime>(
                METASOUND_GET_PARAM_NAME(InputRiseTime),
                InParams.OperatorSettings
            );

            TDataReadReference<FTime> InputFallTime = InputData.GetOrCreateDefaultDataReadReference<FTime>(
                METASOUND_GET_PARAM_NAME(InputFallTime),
                InParams.OperatorSettings
            );

            return MakeUnique<FBoolToAudioOperator>(InParams.OperatorSettings, InputBool, InputRiseTime, InputFallTime);
        }

        virtual void Execute()
        {
            int32 NumFrames = OutputSignal->Num();
            float* OutputDataPtr = OutputSignal->GetData();

            float TargetValue = *InputBool ? 1.0f : 0.0f;

            float RiseTimeSeconds = InputRiseTime->GetSeconds();
            float FallTimeSeconds = InputFallTime->GetSeconds();

            float RiseAlpha = (RiseTimeSeconds > 0.0f) ? FMath::Exp(-1.0f / (RiseTimeSeconds * SampleRate)) : 0.0f;
            float FallAlpha = (FallTimeSeconds > 0.0f) ? FMath::Exp(-1.0f / (FallTimeSeconds * SampleRate)) : 0.0f;

            for (int32 i = 0; i < NumFrames; ++i)
            {
                float OutputSample = PreviousOutputSample;

                if (TargetValue > PreviousOutputSample)
                {
                    OutputSample = RiseAlpha * PreviousOutputSample + (1.0f - RiseAlpha) * TargetValue;
                }
                else if (TargetValue < PreviousOutputSample)
                {
                    OutputSample = FallAlpha * PreviousOutputSample + (1.0f - FallAlpha) * TargetValue;
                }
                else
                {
                    OutputSample = TargetValue;
                }

                OutputDataPtr[i] = OutputSample;
                PreviousOutputSample = OutputSample;
            }
        }

    private:
        FBoolReadRef InputBool;
        FTimeReadRef InputRiseTime;
        FTimeReadRef InputFallTime;
        FAudioBufferWriteRef OutputSignal;
        float PreviousOutputSample;
        float SampleRate;
    };

    class FBoolToAudioNode : public FNodeFacade
    {
    public:
        FBoolToAudioNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FBoolToAudioOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FBoolToAudioNode);
}

#undef LOCTEXT_NAMESPACE