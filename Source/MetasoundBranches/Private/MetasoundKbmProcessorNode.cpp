// Copyright 2025 Charles Matthews.  All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundKbmProcessorNode.h"
#include "MetasoundBranches/Public/MetasoundKbmDataType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_KbmProcessorNode"

namespace Metasound
{
    namespace KbmProcessorTestNodeVertexNames
    {
        METASOUND_PARAM(InputTrigger,         "Process Note",      "Trigger to calculate frequency from KBM.");
        METASOUND_PARAM(InputNote,            "Note",             "MIDI note to process.");
        METASOUND_PARAM(InputKbmData,         "KBM Data",         "KBM tuning data.");

        METASOUND_PARAM(OutputTrigger,        "On Mapped",        "Fires if note was mapped.");
        METASOUND_PARAM(OutputUnmappedTrigger,"On Unmapped",      "Fires if note is not mapped.");
        METASOUND_PARAM(OutputFrequency,      "Frequency",        "Calculated frequency.");
        METASOUND_PARAM(OutputScaleDegree,    "Scale Degree",     "Scale degree index.");
        METASOUND_PARAM(OutputOctave,         "Octave",           "Octave index based on OctaveDegree.");
    }

    class FKbmProcessorTestNodeOperator : public TExecutableOperator<FKbmProcessorTestNodeOperator>
    {
        public:
        FKbmProcessorTestNodeOperator(
            const FOperatorSettings& InSettings,
            const FTriggerReadRef& InTrigger,
            const FInt32ReadRef& InNote,
            const FKbmDataReadRef& InKbmData)
            : Trigger(InTrigger)
            , Note(InNote)
            , KbmData(InKbmData)
            , OnMappedTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , OnUnmappedTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , Frequency(FFloatWriteRef::CreateNew(0.0f))
            , ScaleDegree(FInt32WriteRef::CreateNew(-1))
            , Octave(FInt32WriteRef::CreateNew(0))
        {
            PrecomputedFrequencies.SetNumUninitialized(128);
            PrecomputedDegrees.SetNumUninitialized(128);
            PrecomputedOctaves.SetNumUninitialized(128);
        }
    

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace KbmProcessorTestNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputNote)),
                    TInputDataVertex<MetasoundKbm::FKbmData>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputKbmData))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputUnmappedTrigger)),
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputFrequency)),
                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputScaleDegree)),
                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputOctave))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("Scala KBM Processor"), TEXT("Scala") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("KbmProcessorTestDisplayName", "Scala KBM Processor");
                Metadata.Description = METASOUND_LOCTEXT("KbmProcessorTestDesc", "Retrieves frequency and note information from a Scala-style keyboard map.");
                Metadata.Author = "Charles Matthews";
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Tuning")
                };
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        FDataReferenceCollection GetInputs() const override
        {
            using namespace KbmProcessorTestNodeVertexNames;
            FDataReferenceCollection Inputs;
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTrigger), Trigger);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputNote), Note);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputKbmData), KbmData);
            return Inputs;
        }

        FDataReferenceCollection GetOutputs() const override
        {
            using namespace KbmProcessorTestNodeVertexNames;
            FDataReferenceCollection Outputs;
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputTrigger), OnMappedTrigger);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputUnmappedTrigger), OnUnmappedTrigger);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputFrequency), Frequency);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputScaleDegree), ScaleDegree);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputOctave), Octave);
            return Outputs;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace KbmProcessorTestNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;

            return MakeUnique<FKbmProcessorTestNodeOperator>(
                InParams.OperatorSettings,
                InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTrigger), InParams.OperatorSettings),
                InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputNote), InParams.OperatorSettings),
                InputData.GetOrCreateDefaultDataReadReference<MetasoundKbm::FKbmData>(METASOUND_GET_PARAM_NAME(InputKbmData), InParams.OperatorSettings)
            );
        }

        void Execute()
        {
            OnMappedTrigger->AdvanceBlock();
            OnUnmappedTrigger->AdvanceBlock();

            Trigger->ExecuteBlock(
                [](int32, int32){},
                [this](int32 StartFrame, int32)
                {
                    const int32 NoteIndex = *Note;
                    const int32 MinNote   = KbmData->FirstNote;
                    const int32 MaxNote   = KbmData->LastNote;

                    if (NoteIndex < MinNote || NoteIndex > MaxNote)
                    {
                        *Frequency   = 0.0f;
                        *ScaleDegree = -1;
                        *Octave      = 0;
                        OnUnmappedTrigger->TriggerFrame(StartFrame);
                        return;
                    }

                    const int32 steps        = KbmData->OctaveDegree - 1;
                    const float refFreq      = KbmData->ReferenceFrequency;
                    const int32 refNote      = KbmData->ReferenceNote;
                    const int32 middleNote   = KbmData->MiddleNote;

                    const TArray<float>& cents = KbmData->CentValues;
                    const TArray<int32>& degs  = KbmData->ScaleDegrees;

                    float octaveCents = cents.IsValidIndex(steps) ? cents[steps] : 1200.f;
                    float octaveRatio = FMath::Pow(2.f, octaveCents / 1200.f);

                    for (int32 i = 0; i < 128; ++i)
                    {
                        PrecomputedFrequencies[i] = 0.0f;
                        PrecomputedDegrees[i]     = -1;
                        PrecomputedOctaves[i]     = 0;

                        const int32 offset = i - middleNote;
                        int32 step = offset % steps;
                        int32 oct  = offset / steps;

                        if (step < 0)
                        {
                            step += steps;
                            oct -= 1;
                        }

                        if (!degs.IsValidIndex(step) || degs[step] < 0)
                            continue;

                            int32 refStep = refNote % steps;
                            if (refStep < 0) refStep += steps;
                            int32 refOct = refNote / steps;
                            
                            float refCents = (refOct * octaveCents) + (cents.IsValidIndex(refStep) ? cents[refStep] : 0.0f);
                            float totalCents = (oct * octaveCents) + (cents.IsValidIndex(step) ? cents[step] : 0.0f);
                            
                            float centOffset = totalCents - refCents;
                            float finalFreq = refFreq * FMath::Pow(2.f, centOffset / 1200.f);

                        PrecomputedFrequencies[i] = finalFreq;
                        PrecomputedDegrees[i]     = degs[step];
                        PrecomputedOctaves[i]     = oct;
                    }

                    float finalFreq = PrecomputedFrequencies[NoteIndex];
                    int32 finalDeg  = PrecomputedDegrees[NoteIndex];
                    int32 finalOct  = PrecomputedOctaves[NoteIndex];

                    if (finalFreq <= 0.f || finalDeg < 0)
                    {
                        *Frequency   = 0.f;
                        *ScaleDegree = -1;
                        *Octave      = 0;
                        OnUnmappedTrigger->TriggerFrame(StartFrame);
                        return;
                    }

                    *Frequency   = finalFreq;
                    *ScaleDegree = finalDeg;
                    *Octave      = finalOct;
                    OnMappedTrigger->TriggerFrame(StartFrame);
                }
            );
        }

    private:
        FTriggerReadRef Trigger;
        FInt32ReadRef   Note;
        FKbmDataReadRef KbmData;

        FTriggerWriteRef OnMappedTrigger;
        FTriggerWriteRef OnUnmappedTrigger;
        FFloatWriteRef   Frequency;
        FInt32WriteRef   ScaleDegree;
        FInt32WriteRef   Octave;

        TArray<float> PrecomputedFrequencies;
        TArray<int32> PrecomputedDegrees;
        TArray<int32> PrecomputedOctaves;
    };

    class FKbmProcessorTestNode : public FNodeFacade
    {
    public:
        FKbmProcessorTestNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FKbmProcessorTestNodeOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FKbmProcessorTestNode);
}

#undef LOCTEXT_NAMESPACE