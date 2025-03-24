// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundKinkNode.h"
#include "MetasoundBranches/Public/Kink.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "MetasoundKinkNode"

namespace Metasound
{
    namespace KinkNodeVertexNames
    {
        METASOUND_PARAM(InputSignal,  "In",        "Audio input.");
        METASOUND_PARAM(InputSlope,   "Slope",     "Base slope factor.");
        METASOUND_PARAM(InputSlopeMod,"Slope Modulation", "Audio-rate slope modulation.");
        METASOUND_PARAM(OutputSignal, "Out",       "Kinked output.");
    }


    class FKinkOperator : public TExecutableOperator<FKinkOperator>
    {
    public:
        FKinkOperator(
            const FOperatorSettings& InSettings,
            const FAudioBufferReadRef& InAudio,
            const FFloatReadRef& InSlope,
            const FAudioBufferReadRef& InSlopeMod
        )
            : AudioIn(InAudio)
            , BaseSlope(InSlope)
            , SlopeMod(InSlopeMod)
            , AudioOut(FAudioBufferWriteRef::CreateNew(InSettings))
            , BlockSize(InSettings.GetNumFramesPerBlock())
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace KinkNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignal)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSlope)),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSlopeMod))
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
                Metadata.ClassName = { TEXT("UE"), TEXT("Kink"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = LOCTEXT("KinkNodeDisplayName", "Kink (Audio)");
                Metadata.Description = LOCTEXT("KinkNodeDesc", "Bends the input signal according to a slope factor.");
                Metadata.Author = TEXT("Charles Matthews");
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = { LOCTEXT("CustomCategory", "Branches") };
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace KinkNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            TDataReadReference<FAudioBuffer> InAudio =
                InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputSignal), InParams.OperatorSettings);
            TDataReadReference<float> InSlope =
                InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputSlope), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InSlopeMod =
                InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputSlopeMod), InParams.OperatorSettings);

            return MakeUnique<FKinkOperator>(InParams.OperatorSettings, InAudio, InSlope, InSlopeMod);
        }
        
        METASOUND_DISABLE_LEGACY_IO()

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace KinkNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal), AudioIn);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSlope), BaseSlope);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSlopeMod), SlopeMod);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace KinkNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), AudioOut);
        }
        
        virtual void Execute()
        {
            const float* InData = AudioIn->GetData();
            const float* SlopeModData = SlopeMod->GetData();
            float* OutData = AudioOut->GetData();

            for (int32 i = 0; i < BlockSize; ++i)
            {
                float CurrentSlope = (*BaseSlope) + SlopeModData[i];
                if (FMath::IsNearlyZero(CurrentSlope))
                    CurrentSlope = 0.000001f;
                OutData[i] = KinkProcess(InData[i], CurrentSlope);
            }
        }

    private:
        FAudioBufferReadRef AudioIn;
        FFloatReadRef BaseSlope;
        FAudioBufferReadRef SlopeMod;
        FAudioBufferWriteRef AudioOut;
        int32 BlockSize;
    };

    class FKinkNode : public FNodeFacade
    {
    public:
        FKinkNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FKinkOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FKinkNode);
}

#undef LOCTEXT_NAMESPACE