// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundShiftRegisterNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundTrigger.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_ShiftRegister"

namespace Metasound
{
    namespace ShiftRegisterNodeVertexNames
    {
        METASOUND_PARAM(InSignal,        "In",              "Input float to the shift register.")
        METASOUND_PARAM(InTrigger,       "Trigger",         "Trigger.")
        METASOUND_PARAM(InReset,         "Reset",           "Reset trigger for the shift register.")
        METASOUND_PARAM(InDefault,       "Default",         "Default float for the shift register.")
        METASOUND_PARAM(InReverse,       "Reverse",         "Reverse flag for the shift register.")
        METASOUND_PARAM(InOutputOnReset, "Output On Reset", "When true, a reset will immediately output default values.")

        METASOUND_PARAM(OutTrigger,      "On Trigger",         "Output trigger following the shift.")
        METASOUND_PARAM(OutReset,        "On Reset",           "Output trigger when Output On Reset is true.")
        METASOUND_PARAM(OutDirChange,    "On Direction Change","Output trigger on direction change.")
        METASOUND_PARAM(OutStage,        "Stage",              "Shifted output at this stage.")
    }

    namespace ShiftRegisterPrivate
    {
        using namespace ShiftRegisterNodeVertexNames;

        static FVertexInterface MakeInterface(int32 NumStages)
        {
            FInputVertexInterface  Ins;
            Ins.Add(TInputDataVertex<float>   (METASOUND_GET_PARAM_NAME_AND_METADATA(InSignal)));
            Ins.Add(TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InTrigger)));
            Ins.Add(TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InReset)));
            Ins.Add(TInputDataVertex<float>   (METASOUND_GET_PARAM_NAME_AND_METADATA(InDefault)));
            Ins.Add(TInputDataVertex<bool>    (METASOUND_GET_PARAM_NAME_AND_METADATA(InReverse)));
            Ins.Add(TInputDataVertex<bool>    (METASOUND_GET_PARAM_NAME_AND_METADATA(InOutputOnReset)));

            FOutputVertexInterface Outs;
            Outs.Add(TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutTrigger)));
            Outs.Add(TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutReset)));
            Outs.Add(TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutDirChange)));

            for (int32 i = 0; i < NumStages; ++i)
            {
                const FString Label = FString::Printf(TEXT("%s %d"), *METASOUND_GET_PARAM_NAME(OutStage).ToString(), i+1);
                Outs.Add(TOutputDataVertex<float>(FName(Label), FDataVertexMetadata{}));
            }

            return FVertexInterface(MoveTemp(Ins), MoveTemp(Outs));
        }

        struct FShiftRegisterOperatorData : public TOperatorData<FShiftRegisterOperatorData>
        {
            static const FLazyName OperatorDataTypeName;
            int32 NumStages = 8;
            FShiftRegisterOperatorData(int32 InStages) : NumStages(InStages) {}
        };

        const FLazyName FShiftRegisterOperatorData::OperatorDataTypeName = "ShiftRegisterOpData";

        class FShiftRegisterOp : public TExecutableOperator<FShiftRegisterOp>
        {
        public:
            FShiftRegisterOp(const FOperatorSettings& S,
                             int32 NumStages,
                             const FBuildOperatorParams& P)
                : In(P.InputData.GetOrCreateDefaultDataReadReference<float>   (METASOUND_GET_PARAM_NAME(InSignal), S))
                , TrigIn(P.InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InTrigger), S))
                , ResetIn(P.InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InReset), S))
                , DefaultVal(P.InputData.GetOrCreateDefaultDataReadReference<float>   (METASOUND_GET_PARAM_NAME(InDefault), S))
                , Reverse   (P.InputData.GetOrCreateDefaultDataReadReference<bool>    (METASOUND_GET_PARAM_NAME(InReverse), S))
                , OutputOnReset(P.InputData.GetOrCreateDefaultDataReadReference<bool> (METASOUND_GET_PARAM_NAME(InOutputOnReset), S))
            {
                for (int32 i = 0; i < NumStages; ++i)
                {
                    OutVals.Add(FFloatWriteRef::CreateNew(0.f));
                    Values.Add(0.f);
                }

                TrigOut      = FTriggerWriteRef::CreateNew(S);
                ResetOut     = FTriggerWriteRef::CreateNew(S);
                DirChangeOut = FTriggerWriteRef::CreateNew(S);
                PrevReverse  = *Reverse;
            }

            void BindInputs(FInputVertexInterfaceData& D) override
            {
                D.BindReadVertex(METASOUND_GET_PARAM_NAME(InSignal),        In);
                D.BindReadVertex(METASOUND_GET_PARAM_NAME(InTrigger),       TrigIn);
                D.BindReadVertex(METASOUND_GET_PARAM_NAME(InReset),         ResetIn);
                D.BindReadVertex(METASOUND_GET_PARAM_NAME(InDefault),       DefaultVal);
                D.BindReadVertex(METASOUND_GET_PARAM_NAME(InReverse),       Reverse);
                D.BindReadVertex(METASOUND_GET_PARAM_NAME(InOutputOnReset), OutputOnReset);
            }

            void BindOutputs(FOutputVertexInterfaceData& D) override
            {
                D.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutTrigger),   TrigOut);
                D.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutReset),     ResetOut);
                D.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutDirChange), DirChangeOut);

                for (int32 i = 0; i < OutVals.Num(); ++i)
                {
                    const FString Label = FString::Printf(TEXT("%s %d"), *METASOUND_GET_PARAM_NAME(OutStage).ToString(), i+1);
                    D.BindReadVertex(FName(Label), OutVals[i]);
                }
            }

            void Execute()
            {
                TrigOut->AdvanceBlock();
                ResetOut->AdvanceBlock();
                DirChangeOut->AdvanceBlock();

                ResetIn->ExecuteBlock([](int32, int32) {},
                [&](int32 F, int32)
                {
                    for (float& v : Values)
                        v = *DefaultVal;

                    if (*OutputOnReset)
                        ResetOut->TriggerFrame(F);
                });

                if (*Reverse != PrevReverse)
                {
                    DirChangeOut->TriggerFrame(0);
                    PrevReverse = *Reverse;
                }

                TrigIn->ExecuteBlock([](int32, int32) {},
                [&](int32 F, int32)
                {
                    if (*Reverse)
                    {
                        for (int32 i = Values.Num()-1; i > 0; --i)
                            Values[i] = Values[i-1];
                        Values[0] = *In;
                    }
                    else
                    {
                        for (int32 i = 0; i < Values.Num()-1; ++i)
                            Values[i] = Values[i+1];
                        Values.Last() = *In;
                    }
                    TrigOut->TriggerFrame(F);
                });

                for (int32 i = 0; i < Values.Num(); ++i)
                    *OutVals[i] = Values[i];
            }

        private:
            FFloatReadRef In, DefaultVal;
            TDataReadReference<bool> Reverse, OutputOnReset;
            FTriggerReadRef TrigIn, ResetIn;
            FTriggerWriteRef TrigOut, ResetOut, DirChangeOut;

            TArray<float> Values;
            TArray<FFloatWriteRef> OutVals;
            bool PrevReverse = false;
        };

        class FShiftRegisterNode : public FNodeFacade
        {
        public:
            FShiftRegisterNode(const FNodeInitData& Init)
                : FNodeFacade(Init.InstanceName, Init.InstanceID,
                    TFacadeOperatorClassWithData<FShiftRegisterOp, FShiftRegisterOperatorData>()) {}
        };
    }

    TInstancedStruct<FMetasoundFrontendClassInterface>
    FMetaSoundShiftRegisterNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass&) const
    {
        return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
            FMetasoundFrontendClassInterface::GenerateClassInterface(
                ShiftRegisterPrivate::MakeInterface(NumStages)));
    }

    TSharedPtr<const Metasound::IOperatorData>
    FMetaSoundShiftRegisterNodeConfiguration::GetOperatorData() const
    {
        return MakeShared<ShiftRegisterPrivate::FShiftRegisterOperatorData>(NumStages);
    }

    METASOUND_REGISTER_NODE_AND_CONFIGURATION(
        ShiftRegisterPrivate::FShiftRegisterNode,
        FMetaSoundShiftRegisterNodeConfiguration)
}

#undef LOCTEXT_NAMESPACE