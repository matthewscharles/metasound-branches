// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundGetBlockDurationNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundGetBlockDurationNode"

namespace Metasound
{
    namespace GetBlockDurationNodeVertexNames
    {
        METASOUND_PARAM(OutputBlockDuration, "Block Duration", "Duration of the current block in seconds.");
    }

    class FGetBlockDurationOperator : public TExecutableOperator<FGetBlockDurationOperator>
    {
    public:
        FGetBlockDurationOperator(const FOperatorSettings& InSettings, int32 InSampleRate)
            : SampleRate(InSampleRate)
            , OutputBlockDuration(FTimeWriteRef::CreateNew(FTime()))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace GetBlockDurationNodeVertexNames;
            static const FVertexInterface Interface(
                FInputVertexInterface(),
                FOutputVertexInterface(
                    TOutputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputBlockDuration))
                )
            );
            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("Get Block Duration"), TEXT("Time") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("GetBlockDurationDisplayName", "Get Block Duration");
                Metadata.Description = METASOUND_LOCTEXT("GetBlockDurationDesc", "Outputs the duration of the current block in seconds.");
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
            FDataReferenceCollection InputDataReferences;
            return InputDataReferences;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace GetBlockDurationNodeVertexNames;
            FDataReferenceCollection OutputDataReferences;
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputBlockDuration), OutputBlockDuration);
            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            int32 SampleRate = InParams.OperatorSettings.GetActualBlockRate();
            return MakeUnique<FGetBlockDurationOperator>(InParams.OperatorSettings, SampleRate);
        }

        virtual void Execute()
        {
            double BlockDurationSeconds = (SampleRate > 0) ? (1.0 / static_cast<double>(SampleRate)) : 0.0;
            *OutputBlockDuration = FTime(BlockDurationSeconds);
        }

    private:
        int32 SampleRate;
        FTimeWriteRef OutputBlockDuration;
    };

    class FGetBlockDurationNode : public FNodeFacade
    {
    public:
        FGetBlockDurationNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FGetBlockDurationOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FGetBlockDurationNode);
}

#undef LOCTEXT_NAMESPACE