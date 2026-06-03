// Copyright 2026 Charles Matthews. All Rights Reserved.

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
#include "MetasoundTime.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include "Misc/ScopeLock.h"
#include "MetasoundArrayTypeTraits.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundFrontend"

namespace Metasound
{
    namespace ArrayInterpolateNodePrivate
    {
        template<typename ElementType>
        struct TInterpolator
        {
            static ElementType Interpolate(const ElementType& InA, const ElementType& InB, const float InAlpha)
            {
                return FMath::Lerp(InA, InB, InAlpha);
            }
        };

        template<>
        struct TInterpolator<int32>
        {
            static int32 Interpolate(const int32& InA, const int32& InB, const float InAlpha)
            {
                return FMath::RoundToInt(FMath::Lerp(static_cast<float>(InA), static_cast<float>(InB), InAlpha));
            }
        };

        template<>
        struct TInterpolator<FTime>
        {
            static FTime Interpolate(const FTime& InA, const FTime& InB, const float InAlpha)
            {
                return FTime::FromSeconds(FMath::Lerp(InA.GetSeconds(), InB.GetSeconds(), static_cast<double>(InAlpha)));
            }
        };
    }

    namespace ArrayInterpolateNodeVertexNames
    {
        METASOUND_PARAM(InputTriggerInterpolate, "Interpolate", "Trigger to interpolate between the two arrays.")
        METASOUND_PARAM(InputArrayA, "Array A", "First input array.")
        METASOUND_PARAM(InputArrayB, "Array B", "Second input array.")
        METASOUND_PARAM(InputAlpha, "Alpha", "Interpolation amount. 0 outputs Array A, 1 outputs Array B.")

        METASOUND_PARAM(OutputTriggerOnInterpolate, "On Interpolate", "Triggers when the interpolated array is output.")
        METASOUND_PARAM(OutputArray, "Array", "The interpolated array. If the input arrays differ in length, the shorter array repeats its last value.")
    }

    template<typename ArrayType>
    class TArrayInterpolateOperator : public TExecutableOperator<TArrayInterpolateOperator<ArrayType>>
    {
    public:
        using FArrayDataReadReference = TDataReadReference<ArrayType>;
        using FElementType = typename MetasoundArrayNodesPrivate::TArrayElementType<ArrayType>::Type;
        using FAlphaDataReadReference = TDataReadReference<float>;
        using FArrayDataWriteReference = TDataWriteReference<ArrayType>;

        static const FVertexInterface& GetDefaultInterface()
        {
            using namespace ArrayInterpolateNodeVertexNames;
            static const FVertexInterface DefaultInterface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerInterpolate)),
                    TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputArrayA)),
                    TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputArrayB)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAlpha), 0.5f)
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnInterpolate)),
                    TOutputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputArray))
                )
            );
            return DefaultInterface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                return MetasoundArrayNodesPrivate::CreateArrayNodeClassMetadata(
                    GetMetasoundDataTypeName<ArrayType>(),
                    TEXT("Interpolate"),
                    METASOUND_LOCTEXT_FORMAT("ArrayOpInterpolateDisplayNamePattern", "Interpolate ({0})", GetMetasoundDataTypeDisplayText<ArrayType>()),
                    LOCTEXT("ArrayOpInterpolateDesc", "Outputs an array interpolated between Array A and Array B when triggered."),
                    GetDefaultInterface(),
                    1,
                    0,
                    false
                );
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            const_cast<FNodeClassMetadata&>(Metadata).Author = TEXT("Charles Matthews");
            const_cast<FNodeClassMetadata&>(Metadata).PromptIfMissing = PluginNodeMissingPrompt;
            const_cast<FNodeClassMetadata&>(Metadata).CategoryHierarchy = {
                METASOUND_LOCTEXT("Custom", "Branches"),
                METASOUND_LOCTEXT("CustomSub", "Array")
            };
            const_cast<FNodeClassMetadata&>(Metadata).Keywords = TArray<FText>();
            return Metadata;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
        {
            using namespace ArrayInterpolateNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<FTrigger> InTriggerInterpolate = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputTriggerInterpolate),
                InParams.OperatorSettings
            );
            FArrayDataReadReference InInputArrayA = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(
                METASOUND_GET_PARAM_NAME(InputArrayA),
                InParams.OperatorSettings
            );
            FArrayDataReadReference InInputArrayB = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(
                METASOUND_GET_PARAM_NAME(InputArrayB),
                InParams.OperatorSettings
            );
            FAlphaDataReadReference InAlpha = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputAlpha),
                InParams.OperatorSettings
            );

            return MakeUnique<TArrayInterpolateOperator>(InParams, InTriggerInterpolate, InInputArrayA, InInputArrayB, InAlpha);
        }

        TArrayInterpolateOperator(
            const FBuildOperatorParams& InParams,
            const TDataReadReference<FTrigger>& InTriggerInterpolate,
            const FArrayDataReadReference& InInputArrayA,
            const FArrayDataReadReference& InInputArrayB,
            const FAlphaDataReadReference& InAlpha)
            : TriggerInterpolate(InTriggerInterpolate)
            , InputArrayA(InInputArrayA)
            , InputArrayB(InInputArrayB)
            , Alpha(InAlpha)
            , TriggerOnInterpolate(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
            , OutArray(TDataWriteReferenceFactory<ArrayType>::CreateAny(InParams.OperatorSettings))
        {
            *OutArray = *InputArrayA;
        }

        virtual ~TArrayInterpolateOperator() = default;

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ArrayInterpolateNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerInterpolate), TriggerInterpolate);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputArrayA), InputArrayA);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputArrayB), InputArrayB);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAlpha), Alpha);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ArrayInterpolateNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnInterpolate), TriggerOnInterpolate);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputArray), OutArray);
        }

        METASOUND_DISABLE_LEGACY_IO()

        void Execute()
        {
            TriggerOnInterpolate->AdvanceBlock();

            TriggerInterpolate->ExecuteBlock(
                [](int32, int32) {},
                [this](int32 StartFrame, int32)
                {
                    const ArrayType& ArrayA = *InputArrayA;
                    const ArrayType& ArrayB = *InputArrayB;
                    const int32 NumA = ArrayA.Num();
                    const int32 NumB = ArrayB.Num();

                    if (NumA == 0 && NumB == 0)
                    {
                        OutArray->Reset();
                        TriggerOnInterpolate->TriggerFrame(StartFrame);
                        return;
                    }

                    const int32 OutputNum = FMath::Max(NumA, NumB);
                    OutArray->SetNum(OutputNum);

                    for (int32 Index = 0; Index < OutputNum; ++Index)
                    {
                        const FElementType& AValue = (NumA > 0)
                            ? ArrayA[FMath::Min(Index, NumA - 1)]
                            : ArrayB[FMath::Min(Index, NumB - 1)];
                        const FElementType& BValue = (NumB > 0)
                            ? ArrayB[FMath::Min(Index, NumB - 1)]
                            : ArrayA[FMath::Min(Index, NumA - 1)];

                        (*OutArray)[Index] = ArrayInterpolateNodePrivate::TInterpolator<FElementType>::Interpolate(AValue, BValue, *Alpha);
                    }

                    TriggerOnInterpolate->TriggerFrame(StartFrame);
                }
            );
        }

    private:
        TDataReadReference<FTrigger> TriggerInterpolate;
        FArrayDataReadReference InputArrayA;
        FArrayDataReadReference InputArrayB;
        FAlphaDataReadReference Alpha;

        TDataWriteReference<FTrigger> TriggerOnInterpolate;
        FArrayDataWriteReference OutArray;
    };

    template<typename ArrayType>
    class TArrayInterpolateNode : public FNodeFacade
    {
    public:
        static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return TArrayInterpolateOperator<ArrayType>::GetNodeInfo();
        }

        TArrayInterpolateNode(FNodeData InInitData)
            : FNodeFacade(InInitData, MakeShared<const FNodeClassMetadata>(TArrayInterpolateOperator<ArrayType>::GetNodeInfo()), TFacadeOperatorClass<TArrayInterpolateOperator<ArrayType>>())
        {
        }

        virtual ~TArrayInterpolateNode() = default;
    };
}

#undef LOCTEXT_NAMESPACE
