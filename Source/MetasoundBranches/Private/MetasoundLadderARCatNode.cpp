// Copyright 2025 Charles Matthews.  All Rights Reserved.

#include "MetasoundChannelAgnosticType.h"        // FChannelAgnosticType, FChannelAgnosticTypeReadRef, WriteRef
#include "TypeFamily/ChannelTypeFamily.h"        // FChannelTypeFamily, FTranscoder
#include "DSP/MultiMono.h"                       // TStackArrayOfPointers, MakeMultiMonoPointersFromView


#include "MetasoundBranches/Public/MetasoundLadderARCatNode.h"

#include "DSP/Filter.h"
#include "Math/UnrealMathUtility.h"

#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"


#define LOCTEXT_NAMESPACE "MetasoundLadderArCatNode"

namespace Metasound
{
	/* ---------------- Pin names ---------------- */
	namespace LadderArCatPins
	{
		METASOUND_PARAM(InputCAT,          "In",                 "Multichannel audio (CAT) to filter.");
		METASOUND_PARAM(InputCutoff,       "Cutoff",             "Base cutoff frequency (Hz).");
		METASOUND_PARAM(InputResonance,    "Resonance",          "Base resonance / Q.");
		METASOUND_PARAM(InputCutoffMod,    "Cutoff Mod (AR)",    "Audio-rate modulation signal for cutoff.");
		METASOUND_PARAM(InputResMod,       "Resonance Mod (AR)", "Audio-rate modulation signal for resonance.");
		METASOUND_PARAM(OutputCAT,         "Out",                "Filtered multichannel audio (CAT).");
	}

	/* --------------------------------------------------------------------
	 *  Operator
	 * ------------------------------------------------------------------*/
	class FLadderArCatOperator final : public TExecutableOperator<FLadderArCatOperator>
	{
		using FLadder = Audio::FLadderFilter;
		using FTranscoder = Audio::FChannelTypeFamily::FTranscoder;

	public:
		FLadderArCatOperator(const FBuildOperatorParams&  InParams,
							 FChannelAgnosticTypeReadRef&& InInputCAT,
							 FFloatReadRef&&              InCutoff,
							 FFloatReadRef&&              InResonance,
							 FAudioBufferReadRef&&        InCutoffMod,
							 FAudioBufferReadRef&&        InResMod,
							 FChannelAgnosticTypeWriteRef&& OutCAT,
							 TArray<FLadder>&&            InFilters,
							 FTranscoder&&                InTranscoder)
		: InputCAT     (MoveTemp(InInputCAT))
		, CutoffBase   (MoveTemp(InCutoff))
		, ResonanceBase(MoveTemp(InResonance))
		, CutoffModBuf (MoveTemp(InCutoffMod))
		, ResModBuf    (MoveTemp(InResMod))
		, OutputCAT    (MoveTemp(OutCAT))
		, Filters      (MoveTemp(InFilters))
		, Transcoder   (MoveTemp(InTranscoder))
		, Settings     (InParams.OperatorSettings)
		, NumFrames    (Settings.GetNumFramesPerBlock())
		, Nyquist      (0.5f * Settings.GetSampleRate())
		{}

		/* ----- Vertex interface (static) ----- */
		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace LadderArCatPins;

			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputCAT)),
					TInputDataVertex<float>             (METASOUND_GET_PARAM_NAME_AND_METADATA(InputCutoff)),
					TInputDataVertex<float>             (METASOUND_GET_PARAM_NAME_AND_METADATA(InputResonance)),
					TInputDataVertex<FAudioBuffer>      (METASOUND_GET_PARAM_NAME_AND_METADATA(InputCutoffMod)),
					TInputDataVertex<FAudioBuffer>      (METASOUND_GET_PARAM_NAME_AND_METADATA(InputResMod))
				),
				FOutputVertexInterface(
					TOutputDataVertex<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputCAT))
				));
			return Interface;
		}

		/* ----- Node metadata ----- */
		static const FNodeClassMetadata& GetNodeInfo()
		{
			static const FNodeClassMetadata Metadata = []()
			{
				FNodeClassMetadata M;
				M.ClassName       = { TEXT("UE"), TEXT("Ladder (AR) CAT"), TEXT("Audio") };
				M.MajorVersion    = 1;
				M.MinorVersion    = 0;
				M.DisplayName     = LOCTEXT("LadderArCatDisplayName", "Ladder (AR) – CAT");
				M.Description     = LOCTEXT("LadderArCatDesc",
					"Per-channel ladder filter with audio-rate modulation, preserving the input CAT’s channel layout.");
				M.Author          = TEXT("Charles Matthews");
				M.PromptIfMissing = METASOUND_LOCTEXT("EnablePlugin", "Enable the Branches plugin.");
				M.DefaultInterface= DeclareVertexInterface();
				M.CategoryHierarchy =
				{
					METASOUND_LOCTEXT("Custom",    "Branches"),
					METASOUND_LOCTEXT("CustomSub", "Filters")
				};
				return M;
			}();
			return Metadata;
		}

		/* ----- Factory ----- */
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
		{
			using namespace LadderArCatPins;
			const FInputVertexInterfaceData& InData = InParams.InputData;

			/* Pins --------------------------------------------------- */
			auto InCAT = InData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(
				METASOUND_GET_PARAM_NAME(InputCAT), InParams.OperatorSettings);

			auto Cutoff = InData.GetOrCreateDefaultDataReadReference<float>(
				METASOUND_GET_PARAM_NAME(InputCutoff), InParams.OperatorSettings);

			auto Resonance = InData.GetOrCreateDefaultDataReadReference<float>(
				METASOUND_GET_PARAM_NAME(InputResonance), InParams.OperatorSettings);

			auto CutoffMod = InData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
				METASOUND_GET_PARAM_NAME(InputCutoffMod), InParams.OperatorSettings);

			auto ResMod = InData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
				METASOUND_GET_PARAM_NAME(InputResMod), InParams.OperatorSettings);

			/* Output CAT (same concrete channel type as input) --------*/
			const Audio::FChannelTypeFamily& ChannelType = InCAT->GetType();
			auto OutCAT = FChannelAgnosticTypeWriteRef::CreateNew(ChannelType, InParams.OperatorSettings);

			/* Per-channel filter bank --------------------------------*/
			const int32 NumCh = ChannelType.NumChannels();
			TArray<FLadder> Filters;
			Filters.Reserve(NumCh);
			const float Fs = InParams.OperatorSettings.GetSampleRate();
			for (int32 i = 0; i < NumCh; ++i)
			{
				FLadder& L = Filters.Emplace_GetRef();
				L.Init(Fs, 1 /*channels*/);
			}

			/* Transcoder (pass-through if formats match) --------------*/
			FTranscoder Xcoder = ChannelType.GetTranscoder({ .ToType = ChannelType });

			return MakeUnique<FLadderArCatOperator>(
				InParams,
				MoveTemp(InCAT),
				MoveTemp(Cutoff),
				MoveTemp(Resonance),
				MoveTemp(CutoffMod),
				MoveTemp(ResMod),
				MoveTemp(OutCAT),
				MoveTemp(Filters),
				MoveTemp(Xcoder));
		}

		/* ----- Binding ----- */
		virtual void BindInputs (FInputVertexInterfaceData&  Data) override
		{
			using namespace LadderArCatPins;
			Data.BindReadVertex(METASOUND_GET_PARAM_NAME(InputCAT),      InputCAT);
			Data.BindReadVertex(METASOUND_GET_PARAM_NAME(InputCutoff),   CutoffBase);
			Data.BindReadVertex(METASOUND_GET_PARAM_NAME(InputResonance),ResonanceBase);
			Data.BindReadVertex(METASOUND_GET_PARAM_NAME(InputCutoffMod),CutoffModBuf);
			Data.BindReadVertex(METASOUND_GET_PARAM_NAME(InputResMod),   ResModBuf);
		}
		virtual void BindOutputs(FOutputVertexInterfaceData& Data) override
		{
			using namespace LadderArCatPins;
			Data.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputCAT), OutputCAT);
		}

		/* ----- Main DSP loop ----- */
		virtual void Execute()
		{
			/* 1) Copy / channel-map input -> output (handles mono-to-5.1, etc.) */
			if (Transcoder)
			{
				Audio::TStackArrayOfPointers<const float> Src = Audio::MakeMultiMonoPointersFromView(
					InputCAT->GetRawMultiMono(), NumFrames, InputCAT->NumChannels());

				Audio::TStackArrayOfPointers<float> Dst = Audio::MakeMultiMonoPointersFromView(
					OutputCAT->GetRawMultiMono(), NumFrames, OutputCAT->NumChannels());

				Transcoder(Src, Dst, NumFrames);
			}
			else
			{
				*OutputCAT = *InputCAT; // same format, cheap copy
			}

			/* 2) Apply per-channel ladder filter in place on OutputCAT */
			const float* CutoffMod = CutoffModBuf->GetData();
			const float* ResMod    = ResModBuf->GetData();

			for (int32 ch = 0; ch < Filters.Num(); ++ch)
			{
				float* Chan = OutputCAT->GetRawMultiMono()[ch];
				FLadder& L  = Filters[ch];

				for (int32 i = 0; i < NumFrames; ++i)
				{
					const float Fc = FMath::Clamp(*CutoffBase + CutoffMod[i],  0.f, Nyquist);
					const float Q  = FMath::Clamp(*ResonanceBase + ResMod[i],  1.f, 10.f);

					L.SetFrequency(Fc);
					L.SetQ(Q);
					L.Update();

					float Sample = Chan[i];
					L.ProcessAudio(&Sample, 1, &Sample);
					Chan[i] = Sample;
				}
			}
		}

	private:
		/* Pins */
		FChannelAgnosticTypeReadRef   InputCAT;
		FFloatReadRef                 CutoffBase;
		FFloatReadRef                 ResonanceBase;
		FAudioBufferReadRef           CutoffModBuf;
		FAudioBufferReadRef           ResModBuf;
		FChannelAgnosticTypeWriteRef  OutputCAT;

		/* State */
		TArray<FLadder>               Filters;
		FTranscoder                   Transcoder;
		const FOperatorSettings       Settings;
		const int32                   NumFrames;
		const float                   Nyquist;
	};

	/* --------------------------------------------------------------------
	 *  Facade & registration
	 * ------------------------------------------------------------------*/
	using FLadderArCatNode = TNodeFacade<FLadderArCatOperator>;

	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FLadderArCatNode,
	                                          FMetaSoundLadderArCatNodeConfiguration);
} // namespace Metasound

#undef LOCTEXT_NAMESPACE