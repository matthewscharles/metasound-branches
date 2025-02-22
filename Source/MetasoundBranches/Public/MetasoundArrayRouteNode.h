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
            const FBuildOperatorParams& InParams,
            const TDataReadReference<FTrigger>& InSet0,
            const TDataReadReference<FTrigger>& InSet1,
            const FArrayDataReadReference& InArray0,
            const FArrayDataReadReference& InArray1
        )
            : TriggerSet0(InSet0)
            , TriggerSet1(InSet1)
            , InputArray0(InArray0)
            , InputArray1(InArray1)
            , TriggerOnSet0(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
            , TriggerOnSet1(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
            , OutArray(TDataWriteReferenceFactory<ArrayType>::CreateAny(InParams.OperatorSettings))

            // "Live" copies, updated every block to track late-arriving changes
            , LiveArray0(*InArray0)
            , LiveArray1(*InArray1)

            // "Held" arrays store the last triggered (sample-and-hold) values
            , HeldArray0(*InArray0)
            , HeldArray1(*InArray1)

            // Waiting & Retry
            , RetryCount0(0)
            , RetryCount1(0)
            , bWaitingForUpdate0(false)
            , bWaitingForUpdate1(false)
            , StoredTriggerFrame0(0)
            , StoredTriggerFrame1(0)
        {
            // Initially route Array0 to output, just like a typical sample-and-hold node defaulting to one input.
            *OutArray = HeldArray0;
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

        template <typename T>
        FString ArrayToString(const TArray<T>& Array)
        {
            FString ArrayString;
            for (const T& Value : Array)
            {
                ArrayString += LexToString(Value) + TEXT(" ");
            }
            return ArrayString.TrimEnd(); // Remove trailing space
        }
        
        void Execute()
        {
            // Advance triggers
            TriggerOnSet0->AdvanceBlock();
            TriggerOnSet1->AdvanceBlock();

            // Update "live" arrays every block to capture any changes, even if there's no trigger
            LiveArray0 = *InputArray0;
            LiveArray1 = *InputArray1;

            // -- Process any waiting states first --
            if (bWaitingForUpdate0)
            {
                bool bNewChangeDetected = (LiveArray0 != HeldArray0);
                if (bNewChangeDetected)
                {
                    // If we detect a new change while waiting, sample it immediately
                    HeldArray0 = LiveArray0;
                    *OutArray  = HeldArray0; 
                    TriggerOnSet0->TriggerFrame(StoredTriggerFrame0);

                    UE_LOG(LogTemp, Warning, TEXT("Route: (Waiting) Set 0 fired with new value %s"), *ArrayToString(HeldArray0));
                    bWaitingForUpdate0 = false;
                    RetryCount0 = 0;
                }
                else if (RetryCount0 < MaxRetries)
                {
                    RetryCount0++;
                    UE_LOG(LogTemp, Warning, TEXT("Route: (Waiting) Set 0, retry %d/%d"), RetryCount0, MaxRetries);
                }
                else
                {
                    // Timeout: sample the current LiveArray0 anyway
                    HeldArray0 = LiveArray0;
                    *OutArray  = HeldArray0;
                    TriggerOnSet0->TriggerFrame(StoredTriggerFrame0);

                    UE_LOG(LogTemp, Warning, TEXT("Route: (Timeout) Set 0 fired with value %s"), *ArrayToString(HeldArray0));
                    bWaitingForUpdate0 = false;
                    RetryCount0 = 0;
                }
            }

            if (bWaitingForUpdate1)
            {
                bool bNewChangeDetected = (LiveArray1 != HeldArray1);
                if (bNewChangeDetected)
                {
                    HeldArray1 = LiveArray1;
                    *OutArray  = HeldArray1;
                    TriggerOnSet1->TriggerFrame(StoredTriggerFrame1);

                    UE_LOG(LogTemp, Warning, TEXT("Route: (Waiting) Set 1 fired with new value %s"), *ArrayToString(HeldArray1));
                    bWaitingForUpdate1 = false;
                    RetryCount1 = 0;
                }
                else if (RetryCount1 < MaxRetries)
                {
                    RetryCount1++;
                    UE_LOG(LogTemp, Warning, TEXT("Route: (Waiting) Set 1, retry %d/%d"), RetryCount1, MaxRetries);
                }
                else
                {
                    HeldArray1 = LiveArray1;
                    *OutArray  = HeldArray1;
                    TriggerOnSet1->TriggerFrame(StoredTriggerFrame1);

                    UE_LOG(LogTemp, Warning, TEXT("Route: (Timeout) Set 1 fired with value %s"), *ArrayToString(HeldArray1));
                    bWaitingForUpdate1 = false;
                    RetryCount1 = 0;
                }
            }

            // -- Process new triggers --
            TriggerSet0->ExecuteBlock(
                [this](int32, int32) {},
                [this](int32 TriggerFrame, int32)
                {
                    // If the live array is different from the last "held" (sampled) array, fire immediately
                    bool bImmediateChange = (LiveArray0 != HeldArray0);
                    if (bImmediateChange)
                    {
                        HeldArray0 = LiveArray0;
                        *OutArray  = HeldArray0;
                        TriggerOnSet0->TriggerFrame(TriggerFrame);

                        UE_LOG(LogTemp, Warning, TEXT("Route: Immediate Set 0 fired with new value %s"), *ArrayToString(HeldArray0));
                        bWaitingForUpdate0 = false;
                        RetryCount0        = 0;
                    }
                    else
                    {
                        // No immediate change: wait for a few blocks to see if the array updates
                        bWaitingForUpdate0 = true;
                        StoredTriggerFrame0 = TriggerFrame;
                        RetryCount0 = 0;

                        UE_LOG(LogTemp, Warning, TEXT("Route: Set 0 triggered, no immediate change, waiting..."));
                    }
                }
            );

            TriggerSet1->ExecuteBlock(
                [this](int32, int32) {},
                [this](int32 TriggerFrame, int32)
                {
                    bool bImmediateChange = (LiveArray1 != HeldArray1);
                    if (bImmediateChange)
                    {
                        HeldArray1 = LiveArray1;
                        *OutArray  = HeldArray1;
                        TriggerOnSet1->TriggerFrame(TriggerFrame);

                        UE_LOG(LogTemp, Warning, TEXT("Route: Immediate Set 1 fired with new value %s"), *ArrayToString(HeldArray1));
                        bWaitingForUpdate1 = false;
                        RetryCount1        = 0;
                    }
                    else
                    {
                        bWaitingForUpdate1  = true;
                        StoredTriggerFrame1 = TriggerFrame;
                        RetryCount1         = 0;

                        UE_LOG(LogTemp, Warning, TEXT("Route: Set 1 triggered, no immediate change, waiting..."));
                    }
                }
            );
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
    
        // Final routed output array
        TDataWriteReference<ArrayType> OutArray;

        // "Live" arrays (updated every block to catch changes, even if we don't trigger)
        ArrayType LiveArray0;
        ArrayType LiveArray1;

        // "Held" arrays (sample-and-hold: updated only on successful triggers)
        ArrayType HeldArray0;
        ArrayType HeldArray1;
    
        // Waiting / Retry states for each inlet
        int32 RetryCount0;
        int32 RetryCount1;
        bool  bWaitingForUpdate0;
        bool  bWaitingForUpdate1;
        int32 StoredTriggerFrame0;
        int32 StoredTriggerFrame1;

        static constexpr int32 MaxRetries = 4;  // maximum blocks to wait
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