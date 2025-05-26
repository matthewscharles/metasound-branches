// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundSlewConfigurableNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundPrimitives.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundSlewNode"

using namespace Metasound;

/*────────────────────────  Operator data  ────────────────────────*/
const FLazyName FSlewOperatorData::OperatorDataTypeName = "SlewOperatorData";

/*────────────────────────  Vertex helpers  ────────────────────────*/
namespace SlewNames
{
    METASOUND_PARAM(InSignal , "In"       , "Signal to smooth");
    METASOUND_PARAM(InRise   , "Rise Time", "Rise time (s)");
    METASOUND_PARAM(InFall   , "Fall Time", "Fall time (s)");
    METASOUND_PARAM(OutSignal, "Out"      , "Slewed signal");
}

/*────────────────────────  Interface builder  ─────────────────────*/
static FVertexInterface BuildInterface(int32 NumPins, ESlewMode Mode)
{
    FInputVertexInterface   Inputs;
    FOutputVertexInterface  Outputs;

    for (int32 i = 0; i < NumPins; ++i)
    {
        const FString Suffix = FString::Printf(TEXT(" %d"), i);

        if (Mode == ESlewMode::Control)
        {
            Inputs .Add(TInputDataVertex<float>
                {*SlewNames::InSignal + Suffix, FDataVertexMetadata{}} );
            Outputs.Add(TOutputDataVertex<float>
                {*SlewNames::OutSignal + Suffix, FDataVertexMetadata{}} );
        }
        else
        {
            Inputs .Add(TInputDataVertex<FAudioBuffer>
                {*SlewNames::InSignal + Suffix, FDataVertexMetadata{}} );
            Outputs.Add(TOutputDataVertex<FAudioBuffer>
                {*SlewNames::OutSignal + Suffix, FDataVertexMetadata{}} );
        }
    }

    Inputs.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(SlewNames::InRise)));
    Inputs.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(SlewNames::InFall)));

    return FVertexInterface(MoveTemp(Inputs), MoveTemp(Outputs));
}

/*────────────────────────  Configuration  ─────────────────────────*/
TInstancedStruct<FMetasoundFrontendClassInterface>
FMetaSoundSlewNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass&) const
{
    return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
        FMetasoundFrontendClassInterface::GenerateClassInterface(
            BuildInterface(NumPins, SlewMode)));
}

TSharedPtr<const IOperatorData>
FMetaSoundSlewNodeConfiguration::GetOperatorData() const
{
    return MakeShared<FSlewOperatorData>(SlewMode, NumPins);
}

/*────────────────────────  Float-rate operator  ───────────────────*/
class FSlewFloatOperator : public TExecutableOperator<FSlewFloatOperator>
{
public:
    FSlewFloatOperator(const FOperatorSettings& Settings,
                       TArray<TDataReadReference<float>>  InSignals,
                       TDataReadReference<FTime>          InRise,
                       TDataReadReference<FTime>          InFall)
        : Rise(InRise), Fall(InFall), Inputs(MoveTemp(InSignals))
    {
        for (int32 i = 0; i < Inputs.Num(); ++i)
            Outputs.Add(TDataWriteReferenceFactory<float>::CreateExplicitArgs(Settings));
    }

    /* Bind all pins generated at runtime */
    void BindInputs (FInputVertexInterfaceData&  Data) override { /* not needed */ }
    void BindOutputs(FOutputVertexInterfaceData& Data) override { /* not needed */ }

    void Execute()
    {
        const float RiseAlpha = ComputeAlpha(Rise);
        const float FallAlpha = ComputeAlpha(Fall);

        for (int32 c = 0; c < Inputs.Num(); ++c)
        {
            float Input  = *Inputs[c];
            float& Prev  = PrevOut[c];
            float  Out   = Prev;

            if (Input > Prev)       Out = RiseAlpha * Prev + (1 - RiseAlpha) * Input;
            else if (Input < Prev)  Out = FallAlpha * Prev + (1 - FallAlpha) * Input;

            *Outputs[c] = Out;
            Prev        = Out;
        }
    }

    /* Expose outputs */
    void BindOutputsDynamic(FOutputVertexInterfaceData& Data, ESlewMode Mode, int32 NumPins)
    {
        using namespace SlewNames;
        for (int32 i = 0; i < NumPins; ++i)
            Data.BindWriteVertex(*FString::Printf(TEXT("%s %d"), METASOUND_GET_PARAM_NAME(OutSignal), i), Outputs[i]);
    }

private:
    float ComputeAlpha(const FTimeReadRef& T)
    {
        const float Seconds = T->GetSeconds();
        return Seconds > 0.f ? FMath::Exp(-1.f / (Seconds * 1.f /*sampleRate not needed for control*/)) : 0.f;
    }

    TDataReadReference<FTime>              Rise, Fall;
    TArray<TDataReadReference<float>>      Inputs;
    TArray<TDataWriteReference<float>>     Outputs;
    TArray<float>                          PrevOut; /* default-zero */
};

/*────────────────────────  Audio-rate operator  ───────────────────*/
class FSlewAudioOperator : public TExecutableOperator<FSlewAudioOperator>
{
public:
    FSlewAudioOperator(const FOperatorSettings& Settings,
                       TArray<TDataReadReference<FAudioBuffer>> InSignals,
                       TDataReadReference<FTime>                InRise,
                       TDataReadReference<FTime>                InFall)
        : Rise(InRise), Fall(InFall), Inputs(MoveTemp(InSignals))
    {
        for (int32 i = 0; i < Inputs.Num(); ++i)
        {
            Outputs.Add(FAudioBufferWriteRef::CreateNew(Settings));
            PrevOut.Add(0.f);
        }
        SampleRate = Settings.GetSampleRate();
    }

    void Execute()
    {
        const float RiseAlpha = ComputeAlpha(Rise);
        const float FallAlpha = ComputeAlpha(Fall);

        for (int32 c = 0; c < Inputs.Num(); ++c)
        {
            int32 Num = Inputs[c]->Num();
            Outputs[c]->SetNumUninitialized(Num);

            const float* In  = Inputs[c]->GetData();
            float*       Out = Outputs[c]->GetData();
            float        Prev = PrevOut[c];

            for (int32 i = 0; i < Num; ++i)
            {
                float Val = In[i];
                if (Val > Prev)       Prev = RiseAlpha * Prev + (1 - RiseAlpha) * Val;
                else if (Val < Prev)  Prev = FallAlpha * Prev + (1 - FallAlpha) * Val;
                Out[i] = Prev;
            }
            PrevOut[c] = Prev;
        }
    }

private:
    float ComputeAlpha(const FTimeReadRef& T)
    {
        const float Seconds = T->GetSeconds();
        return Seconds > 0.f ? FMath::Exp(-1.f / (Seconds * SampleRate)) : 0.f;
    }

    TDataReadReference<FTime>                     Rise, Fall;
    TArray<TDataReadReference<FAudioBuffer>>      Inputs;
    TArray<FAudioBufferWriteRef>                  Outputs;
    TArray<float>                                 PrevOut;
    int32                                         SampleRate;
};

/*────────────────────────  Facade & factory  ──────────────────────*/
class FSlewNodeOperator : public IOperatorFactory
{
public:
    static TUniquePtr<IOperator> Create(const FBuildOperatorParams& P, FBuildResults&)
    {
        const FSlewOperatorData* Cfg = CastOperatorData<const FSlewOperatorData>(P.Node.GetOperatorData().Get());
        const int32 NumPins          = Cfg->NumPins;
        const ESlewMode Mode         = Cfg->Mode;

        TArray<TDataReadReference<FTime>> RiseFall; // two shared refs
        RiseFall.Add(P.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(SlewNames::InRise), P.OperatorSettings));
        RiseFall.Add(P.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(SlewNames::InFall), P.OperatorSettings));

        if (Mode == ESlewMode::Control)
        {
            TArray<TDataReadReference<float>>  Ins;
            for (int32 i = 0; i < NumPins; ++i)
                Ins.Add(P.InputData.GetOrCreateDefaultDataReadReference<float>(
                    *FString::Printf(TEXT("%s %d"), METASOUND_GET_PARAM_NAME(SlewNames::InSignal), i),
                    P.OperatorSettings));

            return MakeUnique<FSlewFloatOperator>(P.OperatorSettings, MoveTemp(Ins), RiseFall[0], RiseFall[1]);
        }
        else
        {
            TArray<TDataReadReference<FAudioBuffer>> Ins;
            for (int32 i = 0; i < NumPins; ++i)
                Ins.Add(P.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                    *FString::Printf(TEXT("%s %d"), METASOUND_GET_PARAM_NAME(SlewNames::InSignal), i),
                    P.OperatorSettings));

            return MakeUnique<FSlewAudioOperator>(P.OperatorSettings, MoveTemp(Ins), RiseFall[0], RiseFall[1]);
        }
    }
};

/*────────────────────────  Node metadata  ─────────────────────────*/
static FNodeClassMetadata BuildMetadata()
{
    FNodeClassMetadata M;
    M.ClassName       = { TEXT("Branches"), TEXT("Slew"), TEXT("") };
    M.MajorVersion    = 2;          
    M.MinorVersion    = 0;
    M.DisplayName     = LOCTEXT("SlewDisplay", "Slew");
    M.Description     = LOCTEXT("SlewDesc",   "Smooth signal with separate rise/fall; AR or KR.");
    M.Author          = TEXT("Charles Matthews");
    M.PromptIfMissing = LOCTEXT("MissingPrompt", "Enable MetaSound Branches.");
    M.DefaultInterface= BuildInterface(1, ESlewMode::Audio);
    M.CategoryHierarchy =
    {
        LOCTEXT("CatBranches", "Branches"),
        LOCTEXT("CatFilters" , "Filters")
    };
    return M;
}

/*────────────────────────  Facade  ────────────────────────────────*/
class FSlewNode : public FNodeFacade
{
public:
    FSlewNode(const FNodeInitData& Init)
        : FNodeFacade(Init.InstanceName, Init.InstanceID,
                      BuildMetadata(),
                      MakeShared<FSlewNodeOperator>())
    {}
};

/*────────────────────────  Registration  ─────────────────────────*/
using FSlewConfigurableNode = FSlewNode;
METASOUND_REGISTER_NODE_AND_CONFIGURATION(FSlewConfigurableNode, FMetaSoundSlewNodeConfiguration)

#undef LOCTEXT_NAMESPACE