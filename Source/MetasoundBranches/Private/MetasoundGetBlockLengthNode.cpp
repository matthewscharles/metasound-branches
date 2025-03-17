// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundGetBlockLengthNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundGetBlockLengthNode"

namespace Metasound
{
    namespace GetBlockLengthNodeVertexNames
    {
        METASOUND_PARAM(OutputBlockLength, "Block Length", "Length of the current block in seconds.");
    }

    class FGetBlockLengthOperator : public TExecutableOperator<FGetBlockLengthOperator>
    {
    public:
        FGetBlockLengthOperator(const FOperatorSettings& InSettings, int32 InSampleRate)
            : SampleRate(InSampleRate)
            , OutputBlockLength(FTimeWriteRef::CreateNew(FTime()))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace GetBlockLengthNodeVertexNames;
            static const FVertexInterface Interface(
                FInputVertexInterface(),
                FOutputVertexInterface(
                    TOutputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputBlockLength))
                )
            );
            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("Get Block Length"), TEXT("Time") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("GetBlockLengthDisplayName", "Get Block Length");
                Metadata.Description = METASOUND_LOCTEXT("GetBlockLengthDesc", "Outputs the length of the current block in seconds.");
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
            using namespace GetBlockLengthNodeVertexNames;
            FDataReferenceCollection OutputDataReferences;
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputBlockLength), OutputBlockLength);
            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            int32 SampleRate = InParams.OperatorSettings.GetActualBlockRate();
            return MakeUnique<FGetBlockLengthOperator>(InParams.OperatorSettings, SampleRate);
        }

        virtual void Execute()
        {
            double BlockLengthSeconds = (SampleRate > 0) ? (1.0 / static_cast<double>(SampleRate)) : 0.0;
            *OutputBlockLength = FTime(BlockLengthSeconds);
        }

    private:
        int32 SampleRate;
        FTimeWriteRef OutputBlockLength;
    };

    class FGetBlockLengthNode : public FNodeFacade
    {
    public:
        FGetBlockLengthNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FGetBlockLengthOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FGetBlockLengthNode);
}

#undef LOCTEXT_NAMESPACE