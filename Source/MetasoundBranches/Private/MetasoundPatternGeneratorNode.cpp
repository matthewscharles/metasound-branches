// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundPatternGeneratorNode.h"
#include "MetasoundBranches/Public/MetasoundPatternStream.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_PatternGeneratorNode"

namespace Metasound
{
    namespace PatternGeneratorNodeVertexNames
    {
        METASOUND_PARAM(InputPeriod,          "Period",           "Base time duration for triggers.");
        METASOUND_PARAM(InputTimeMultipliers, "Time Multipliers", "Array of multipliers for the base period.");
        METASOUND_PARAM(InputSetPeriodToTotal,"Set Period To Total", "If true, the entire loop uses 'Period' as total duration.");
        METASOUND_PARAM(InputActive,          "Active",           "Enable generation.");
        METASOUND_PARAM(OutputTrigger,        "On Event",         "Trigger output when a new event is generated.");
        METASOUND_PARAM(OutputCurrentIndex,   "Current Index",    "The current index in the time multipliers array.");
        METASOUND_PARAM(OutputTimeMultiplier, "Time Multiplier",  "The current time multiplier.");
        METASOUND_PARAM(OutputStepDuration,   "Step Duration",    "The calculated time duration for the current step.");
        METASOUND_PARAM(OutputPatternStream,  "Pattern Stream",   "Stream output containing the generated events.");
    }

    class FPatternGeneratorOperator : public TExecutableOperator<FPatternGeneratorOperator>
    {
    public:
        FPatternGeneratorOperator(const FOperatorSettings& InSettings,
                                  const FTimeReadRef& InPeriod,
                                  const TDataReadReference<TArray<float>>& InTimeMultipliers,
                                  const TDataReadReference<bool>& InSetPeriodToTotal,
                                  const TDataReadReference<bool>& InActive)
            : InputPeriod(InPeriod)
            , InputTimeMultipliers(InTimeMultipliers)
            , bUseTotal(InSetPeriodToTotal)
            , bActive(InActive)
            , OnGenerateTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , OutCurrentIndex(FInt32WriteRef::CreateNew(0))
            , OutTimeMultiplier(FFloatWriteRef::CreateNew(0.0f))
            , OutStepDuration(FTimeWriteRef::CreateNew(FTime(0.0f)))
            , OutPatternStream(FPatternStreamWriteRef::CreateNew(MetasoundPattern::FPatternStream()))
            , SampleRate(InSettings.GetSampleRate())
            , NumFrames(InSettings.GetNumFramesPerBlock())
            , SampleCounter(0, SampleRate)
            , CurrentIndex(0)
            , CachedTotalMultiplier(0.0f)
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace PatternGeneratorNodeVertexNames;
            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPeriod)),
                    TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTimeMultipliers)),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSetPeriodToTotal)),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputActive))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger)),
                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputCurrentIndex)),
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTimeMultiplier)),
                    TOutputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputStepDuration)),
                    TOutputDataVertex<MetasoundPattern::FPatternStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputPatternStream))
                )
            );
            
            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("Branches"), TEXT("PatternGenerator"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = LOCTEXT("PatternGeneratorDisplayName", "Pattern Generator");
                Metadata.Description = LOCTEXT("PatternGeneratorDesc", "Generates triggers based on an array of time multipliers applied to a base time period.");
                Metadata.Author = TEXT("Charles Matthews");
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Patterns")
                };
                
                return Metadata;
            };
            
            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            
            return Metadata;
        }
        
        METASOUND_DISABLE_LEGACY_IO()
        
        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace PatternGeneratorNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputPeriod), InputPeriod);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTimeMultipliers), InputTimeMultipliers);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSetPeriodToTotal), bUseTotal);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputActive), bActive);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace PatternGeneratorNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputTrigger), OnGenerateTrigger);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputCurrentIndex), OutCurrentIndex);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputTimeMultiplier), OutTimeMultiplier);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputStepDuration), OutStepDuration);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputPatternStream), OutPatternStream);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace PatternGeneratorNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;
            FTimeReadRef PeriodRef = InputData.GetOrCreateDefaultDataReadReference<FTime>(
                METASOUND_GET_PARAM_NAME(InputPeriod), InParams.OperatorSettings
            );
            
            TDataReadReference<TArray<float>> TimeMultipliersRef = InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(
                METASOUND_GET_PARAM_NAME(InputTimeMultipliers), InParams.OperatorSettings
            );
            
            TDataReadReference<bool> SetTotalRef = InputData.GetOrCreateDefaultDataReadReference<bool>(
                METASOUND_GET_PARAM_NAME(InputSetPeriodToTotal), InParams.OperatorSettings
            );
            
            TDataReadReference<bool> ActiveRef = InputData.GetOrCreateDefaultDataReadReference<bool>(
                METASOUND_GET_PARAM_NAME(InputActive), InParams.OperatorSettings
            );
            
            return MakeUnique<FPatternGeneratorOperator>(
                InParams.OperatorSettings, 
                PeriodRef, 
                TimeMultipliersRef, 
                SetTotalRef, 
                ActiveRef
            );
        }

        void Execute()
        {
            OnGenerateTrigger->AdvanceBlock();
            
            if (!(*bActive) || InputTimeMultipliers->Num() == 0)
            {
                return;
            }
            float TotalMultiplier = 0.0f;
            if (*bUseTotal)
            {
                if (CurrentIndex == 0)
                {
                    for (float M : *InputTimeMultipliers)
                    {
                        TotalMultiplier += M;
                    }
                    CachedTotalMultiplier = TotalMultiplier;
                }
                else
                {
                    TotalMultiplier = CachedTotalMultiplier;
                }
            }
            else
            {
                for (float M : *InputTimeMultipliers)
                {
                    TotalMultiplier += M;
                }
            }
            
            float CurrentMultiplier = (*InputTimeMultipliers)[CurrentIndex % InputTimeMultipliers->Num()];
            float StepDurationSec = 0.0f;
            
            if (!(*bUseTotal))
            {
                StepDurationSec = InputPeriod->GetSeconds() * CurrentMultiplier;
            }
            else
            {
                StepDurationSec = InputPeriod->GetSeconds() * (CurrentMultiplier / FMath::Max(0.001f, TotalMultiplier));
            }
            FTime StepDurationTime = FTime(StepDurationSec);
            FSampleCount IntervalInSamples = FSampleCounter::FromTime(StepDurationTime, SampleRate).GetNumSamples();
            IntervalInSamples = FMath::Max<FSampleCount>(1, IntervalInSamples);
            const int32 NumFramesInt = static_cast<int32>(NumFrames);
            
            while ((SampleCounter - NumFramesInt).GetNumSamples() <= 0)
            {
                int32 TriggerFrame = static_cast<int32>(SampleCounter.GetNumSamples());
                TriggerFrame = FMath::Clamp(TriggerFrame, 0, NumFramesInt - 1);
                OnGenerateTrigger->TriggerFrame(TriggerFrame);
                *OutCurrentIndex = CurrentIndex;
                *OutTimeMultiplier = CurrentMultiplier;
                *OutStepDuration = StepDurationTime;
                MetasoundPattern::FPatternEvent NewEvent;
                NewEvent.BlockSampleFrameIndex = TriggerFrame;
                NewEvent.ControlValue = CurrentMultiplier;
                OutPatternStream->AddEvent(NewEvent);
                SampleCounter += IntervalInSamples;
                CurrentIndex = (CurrentIndex + 1) % InputTimeMultipliers->Num();
            }
            
            SampleCounter -= NumFramesInt;
        }

    private:
        FTimeReadRef                           InputPeriod;
        TDataReadReference<TArray<float>>      InputTimeMultipliers;
        TDataReadReference<bool>               bUseTotal;
        TDataReadReference<bool>               bActive;
        FTriggerWriteRef                       OnGenerateTrigger;
        TDataWriteReference<int32>             OutCurrentIndex;
        TDataWriteReference<float>             OutTimeMultiplier;
        FTimeWriteRef                          OutStepDuration;
        FPatternStreamWriteRef                 OutPatternStream;
        float                                  SampleRate;
        float                                  NumFrames;
        FSampleCounter                         SampleCounter;
        int32                                  CurrentIndex;
        float                                  CachedTotalMultiplier;
    };

    class FPatternGeneratorNode : public FNodeFacade
    {
    public:
        FPatternGeneratorNode(const FNodeInitData& InitData)
            : FNodeFacade(
                InitData.InstanceName,
                InitData.InstanceID,
                TFacadeOperatorClass<FPatternGeneratorOperator>()
              )
        {
        }
    };

    METASOUND_REGISTER_NODE(FPatternGeneratorNode);
}

#undef LOCTEXT_NAMESPACE