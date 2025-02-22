// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "Internationalization/Text.h"
#include "MetasoundArrayNodes.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundDataFactory.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundLog.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include "MetasoundArrayTypeTraits.h"
#include "MetasoundTime.h"

#define LOCTEXT_NAMESPACE "MetasoundFrontend"

namespace Metasound
{
    namespace ArrayRouteNodeVertexNames
    {
        METASOUND_PARAM(InputTriggerSet0, "Set 0", "Trigger to route Array 0 to the output.")
        METASOUND_PARAM(InputTriggerSet1, "Set 1", "Trigger to route Array 1 to the output.")

        METASOUND_PARAM(InputArray0, "Array 0", "First input array.")
        METASOUND_PARAM(InputArray1, "Array 1", "Second input array.")

        METASOUND_PARAM(OutputTriggerOnSet0, "On Set 0", "Triggers when Array 0 is routed to the output.")
        METASOUND_PARAM(OutputTriggerOnSet1, "On Set 1", "Triggers when Array 1 is routed to the output.")

        METASOUND_PARAM(OutputArray, "Array", "The currently routed array.")
    }

    template <typename ArrayType>
    class TArrayRouteOperator : public TExecutableOperator<TArrayRouteOperator<ArrayType>>
    {
    public:
        using FArrayDataReadReference  = TDataReadReference<ArrayType>;
        using FArrayDataWriteReference = TDataWriteReference<ArrayType>;
        using FTriggerWriteRef         = TDataWriteReference<FTrigger>;

        static const FVertexInterface& GetDefaultInterface()
        {
            using namespace ArrayRouteNodeVertexNames;

            static const FVertexInterface DefaultInterface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerSet0)),
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerSet1)),
                    TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputArray0)),
                    TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputArray1))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnSet0)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnSet1)),
                    TOutputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputArray))
                )
            );

            return DefaultInterface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata = MetasoundArrayNodesPrivate::CreateArrayNodeClassMetadata(
                    GetMetasoundDataTypeName<ArrayType>(),           
                    TEXT("Array Route"),
                    METASOUND_LOCTEXT_FORMAT("ArrayRouteDisplayNamePattern", "Array Route ({0})", GetMetasoundDataTypeDisplayText<ArrayType>()),
                    LOCTEXT("ArrayRouteDesc", "Routes one of two arrays to a single output, based on triggers."),
                    GetDefaultInterface(),
                    1,  // Major version
                    0,  // Minor version
                    false
                );

                Metadata.Author = TEXT("Charles Matthews");
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.CategoryHierarchy = { LOCTEXT("Custom", "Branches") };
                Metadata.Keywords = TArray<FText>();

                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
        {
            using namespace ArrayRouteNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;

            // Bind triggers
            TDataReadReference<FTrigger> InSet0 = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputTriggerSet0),
                InParams.OperatorSettings
            );

            TDataReadReference<FTrigger> InSet1 = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputTriggerSet1),
                InParams.OperatorSettings
            );

            // Bind arrays
            FArrayDataReadReference InArray0 = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(
                METASOUND_GET_PARAM_NAME(InputArray0),
                InParams.OperatorSettings
            );

            FArrayDataReadReference InArray1 = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(
                METASOUND_GET_PARAM_NAME(InputArray1),
                InParams.OperatorSettings
            );

            return MakeUnique<TArrayRouteOperator>(
                InParams,
                InSet0,
                InSet1,
                InArray0,
                InArray1
            );
        }

        TArrayRouteOperator(
            const FBuildOperatorParams&          InParams,
            const TDataReadReference<FTrigger>&  InSet0,
            const TDataReadReference<FTrigger>&  InSet1,
            const FArrayDataReadReference&       InArray0,
            const FArrayDataReadReference&       InArray1
        )
            : TriggerSet0(InSet0)
            , TriggerSet1(InSet1)
            , InputArray0(InArray0)
            , InputArray1(InArray1)
            , TriggerOnSet0(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
            , TriggerOnSet1(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
            , OutArray(TDataWriteReferenceFactory<ArrayType>::CreateAny(InParams.OperatorSettings))
        {
            *OutArray = *InputArray0;
        }

        virtual ~TArrayRouteOperator() = default;

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ArrayRouteNodeVertexNames;

            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerSet0), TriggerSet0);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerSet1), TriggerSet1);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputArray0),      InputArray0);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputArray1),      InputArray1);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ArrayRouteNodeVertexNames;

            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnSet0), TriggerOnSet0);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnSet1), TriggerOnSet1);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputArray),        OutArray);
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            checkNoEntry();
            return {};
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            checkNoEntry();
            return {};
        }

        void Execute()
        {
            if (*TriggerSet0)
            {
                *OutArray = *InputArray0;

                TriggerOnSet0->ExecuteBlock(
                    [](int32, int32) {},
                    [this](int32 StartFrame, int32) { TriggerOnSet0->TriggerFrame(StartFrame); }
                );
            }

            if (*TriggerSet1)
            {
                *OutArray = *InputArray1;

                TriggerOnSet1->ExecuteBlock(
                    [](int32, int32) {},
                    [this](int32 StartFrame, int32) { TriggerOnSet1->TriggerFrame(StartFrame); }
                );
            }
        }

    private:
        // Input triggers
        TDataReadReference<FTrigger> TriggerSet0;
        TDataReadReference<FTrigger> TriggerSet1;

        // Input arrays
        FArrayDataReadReference InputArray0;
        FArrayDataReadReference InputArray1;

        // Output triggers
        TDataWriteReference<FTrigger> TriggerOnSet0;
        TDataWriteReference<FTrigger> TriggerOnSet1;

        // Routed output array
        TDataWriteReference<ArrayType> OutArray;
    };

    template <typename ArrayType>
    class TArrayRouteNode : public FNodeFacade
    {
    public:
        TArrayRouteNode(const FNodeInitData& InInitData)
            : FNodeFacade(
                InInitData.InstanceName,
                InInitData.InstanceID,
                TFacadeOperatorClass<TArrayRouteOperator<ArrayType>>()
              )
        {
        }

        virtual ~TArrayRouteNode() = default;
    };
}

#undef LOCTEXT_NAMESPACE