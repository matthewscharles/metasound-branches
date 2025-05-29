// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundLadderARNode.h"
#include "Internationalization/Text.h"
#include "MetasoundDataFactory.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesCategories.h"
#include "DSP/Filter.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundAudioBuffer.h" 

#define LOCTEXT_NAMESPACE "MetasoundBranches_LadderAR"

namespace Metasound
{
/* ------------------------------------------------------------------ */
/*  Parameter names                                                   */
/* ------------------------------------------------------------------ */
namespace LadderArParam
{
	METASOUND_PARAM(InputCutoff,        "Cutoff",        "Base cutoff frequency.")
	METASOUND_PARAM(InputResonance,     "Resonance",     "Base resonance.")
	METASOUND_PARAM(InputCutoffMod,     "Cutoff Mod",    "Audio-rate cutoff modulation.")
	METASOUND_PARAM(InputResonanceMod,  "Resonance Mod", "Audio-rate resonance modulation.")
}

/* ------------------------------------------------------------------ */
/*  Vertex-building helpers                                           */
/* ------------------------------------------------------------------ */
namespace LadderArPrivate
{
	/* Make channel pin names “In0”, “In1”, … and “Out0”, … */
	inline FName MakeInName (int32 Idx) { FName N = "In";  N.SetNumber(Idx); return N; }
	inline FName MakeOutName(int32 Idx) { FName N = "Out"; N.SetNumber(Idx); return N; }

#if WITH_EDITOR
	const FText InTooltip  = LOCTEXT("InChanTip",  "Audio input channel.");
	const FText OutTooltip = LOCTEXT("OutChanTip", "Filtered output channel.");
#endif

	FVertexInterface GetVertexInterface(int32 NumCh)
	{
		using namespace LadderArParam;

		FInputVertexInterface  In;
		FOutputVertexInterface Out;

		In.Add(TInputDataVertex<float>      (METASOUND_GET_PARAM_NAME_AND_METADATA(InputCutoff)));
		In.Add(TInputDataVertex<float>      (METASOUND_GET_PARAM_NAME_AND_METADATA(InputResonance)));
		In.Add(TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputCutoffMod)));
		In.Add(TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputResonanceMod)));

		for (int32 i = 0; i < NumCh; ++i)
		{
#if WITH_EDITOR
			const FText DispIn  = FText::Format(LOCTEXT("InDisp",  "In {0}"),  i);
			const FText DispOut = FText::Format(LOCTEXT("OutDisp", "Out {0}"), i);
			In .Add(TInputDataVertex <FAudioBuffer>(MakeInName (i), { InTooltip,  DispIn  }));
			Out.Add(TOutputDataVertex<FAudioBuffer>(MakeOutName(i), { OutTooltip, DispOut }));
#else
			In .Add(TInputDataVertex <FAudioBuffer>(MakeInName (i), {}));
			Out.Add(TOutputDataVertex<FAudioBuffer>(MakeOutName(i), {}));
#endif
		}

		return { MoveTemp(In), MoveTemp(Out) };
	}

	/* Operator-data passed from config to operator */
	class FLadderOpData : public TOperatorData<FLadderOpData>
	{
	public:
		static const FLazyName OperatorDataTypeName;
		int32 NumChannels;
		explicit FLadderOpData(int32 N) : NumChannels(N) {}
	};
	const FLazyName FLadderOpData::OperatorDataTypeName = "LadderAR_OpData";
}

/* ------------------------------------------------------------------ */
/*  Operator                                                          */
/* ------------------------------------------------------------------ */
class FLadderArOperator : public TExecutableOperator<FLadderArOperator>
{
public:
	FLadderArOperator(const FOperatorSettings&     Settings,
	                  TArray<FAudioBufferReadRef>  InCh,
	                  const FFloatReadRef&         InCutoff,
	                  const FFloatReadRef&         InResonance,
	                  const FAudioBufferReadRef&   InCutoffMod,
	                  const FAudioBufferReadRef&   InResMod)
		: Inputs      (MoveTemp(InCh))
		, Cutoff      (InCutoff)
		, Resonance   (InResonance)
		, CutoffMod   (InCutoffMod)
		, ResMod      (InResMod)
		, SampleRate  (Settings.GetSampleRate())
		, BlockSize   (Settings.GetNumFramesPerBlock())
		, MaxCutoffHz (0.5f * SampleRate)
	{
		for (int32 i = 0; i < Inputs.Num(); ++i)
		{
			Outputs.Add(FAudioBufferWriteRef::CreateNew(Settings));
			Filters.Add(Audio::FLadderFilter());
			Filters.Last().Init(SampleRate, 1);
		}
	}

	/* ---------------- Bind ---------------- */
	void BindInputs(FInputVertexInterfaceData& D) override
	{
		using namespace LadderArParam;
		using namespace LadderArPrivate;

		D.BindReadVertex(METASOUND_GET_PARAM_NAME(InputCutoff),       Cutoff);
		D.BindReadVertex(METASOUND_GET_PARAM_NAME(InputResonance),    Resonance);
		D.BindReadVertex(METASOUND_GET_PARAM_NAME(InputCutoffMod),    CutoffMod);
		D.BindReadVertex(METASOUND_GET_PARAM_NAME(InputResonanceMod), ResMod);

		for (int32 i = 0; i < Inputs.Num(); ++i)
		{
			D.BindReadVertex(MakeInName(i), Inputs[i]);
		}
	}

	void BindOutputs(FOutputVertexInterfaceData& D) override
	{
		using namespace LadderArPrivate;
		for (int32 i = 0; i < Outputs.Num(); ++i)
		{
			D.BindWriteVertex(MakeOutName(i), Outputs[i]);
		}
	}

	METASOUND_DISABLE_LEGACY_IO()

	/* ---------------- Factory ------------- */
	static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& P,
	                                            FBuildResults&)
	{
		using namespace LadderArParam;
		using namespace LadderArPrivate;

		const auto& InData = P.InputData;

		auto Cutoff     = InData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputCutoff),       P.OperatorSettings);
		auto Resonance  = InData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputResonance),    P.OperatorSettings);
		auto CutoffMod  = InData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputCutoffMod), P.OperatorSettings);
		auto ResMod     = InData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputResonanceMod), P.OperatorSettings);

		const auto* Cfg = CastOperatorData<const FLadderOpData>(P.Node.GetOperatorData().Get());
		const int32 NumCh = Cfg ? Cfg->NumChannels : 2;

		TArray<FAudioBufferReadRef> InCh;
		InCh.Reserve(NumCh);

		for (int32 i = 0; i < NumCh; ++i)
		{
			InCh.Add(InData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(MakeInName(i), P.OperatorSettings));
		}

		return MakeUnique<FLadderArOperator>(P.OperatorSettings,
		                                     MoveTemp(InCh),
		                                     Cutoff, Resonance,
		                                     CutoffMod, ResMod);
	}

	/* ---------------- Execute ------------- */
	void Execute()
	{
		const float* CutoffM = CutoffMod->GetData();
		const float* ResM    = ResMod->GetData();

		for (int32 ch = 0; ch < Inputs.Num(); ++ch)
		{
			const float* In  = Inputs [ch]->GetData();
			      float* Out = Outputs[ch]->GetData();
			      auto&  Flt = Filters[ch];

			for (int32 i = 0; i < BlockSize; ++i)
			{
				float Fc = *Cutoff    + CutoffM[i];
				float Q  = *Resonance + ResM[i];

				Fc = FMath::Clamp(Fc, 0.0f, MaxCutoffHz);
				Q  = FMath::Clamp(Q , 1.0f, 10.0f);

				Flt.SetFrequency(Fc);
				Flt.SetQ(Q);
				Flt.Update();

				float x = In[i];
				Flt.ProcessAudio(&x, 1, &Out[i]);
			}
		}
	}

private:
	TArray<FAudioBufferReadRef>  Inputs;
	TArray<FAudioBufferWriteRef> Outputs;
	TArray<Audio::FLadderFilter> Filters;

	FFloatReadRef         Cutoff;
	FFloatReadRef         Resonance;
	FAudioBufferReadRef   CutoffMod;
	FAudioBufferReadRef   ResMod;

	const float SampleRate;
	const int32 BlockSize;
	const float MaxCutoffHz;
};

/* ------------------------------------------------------------------ */
/*  Metadata + registration                                           */
/* ------------------------------------------------------------------ */
static const FNodeClassMetadata& GetLadderNodeInfo()
{
	using namespace LadderArPrivate;

	static const FNodeClassMetadata Meta = []()
	{
		FNodeClassMetadata M;
		M.ClassName        = { TEXT("UE"), TEXT("Ladder (AR)"), TEXT("Audio") };
		M.MajorVersion     = 2;
		M.MinorVersion     = 0;
		M.DisplayName      = LOCTEXT("LadderARDisplay", "Ladder (AR)");
		M.Description      = LOCTEXT("LadderARDesc", "Multi-channel ladder filter with audio-rate cutoff and resonance modulation.");
		M.Author           = TEXT("Charles Matthews");
		M.PromptIfMissing  = PluginNodeMissingPrompt;
		M.DefaultInterface = GetVertexInterface(2);  // preview as stereo
		M.CategoryHierarchy = { LOCTEXT("BranchesCat","Branches"), LOCTEXT("FilterCat","Filters") };
		return M;
	}();
	return Meta;
}

using FLadderArNode = TNodeFacade<FLadderArOperator>;
METASOUND_REGISTER_NODE_AND_CONFIGURATION(FLadderArNode, FMetaSoundLadderARNodeConfiguration);

} // namespace Metasound

/* ------------------------------------------------------------------ */
/*  Configuration implementation                                      */
/* ------------------------------------------------------------------ */

TInstancedStruct<FMetasoundFrontendClassInterface>
FMetaSoundLadderARNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass&) const
{
	using namespace Metasound::LadderArPrivate;
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(GetVertexInterface(NumChannels)));
}

TSharedPtr<const Metasound::IOperatorData>
FMetaSoundLadderARNodeConfiguration::GetOperatorData() const
{
	using namespace Metasound::LadderArPrivate;
	return MakeShared<FLadderOpData>(NumChannels);
}

#undef LOCTEXT_NAMESPACE