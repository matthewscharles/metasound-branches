// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundMidiNoteToTimeNode.h"

#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTime.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_MidiNoteToTimeNode"

namespace Metasound
{
	namespace MidiNoteToTimeVertexNames
	{
		METASOUND_PARAM(MidiNote, "MIDI Note", "MIDI note number (float, unquantized).")
		METASOUND_PARAM(ReferenceFrequency, "Reference Frequency", "Reference frequency in Hz.")
		METASOUND_PARAM(ReferencePitch, "Reference Pitch", "Reference note number used by the mapping.")
		METASOUND_PARAM(Divisions, "Divisions", "Equal divisions of the octave used in EDO mode.")
		METASOUND_PARAM(TuningCents0, "Cents 0", "C adjustment in cents.")
		METASOUND_PARAM(TuningCents1, "Cents 1", "C# / Db adjustment in cents.")
		METASOUND_PARAM(TuningCents2, "Cents 2", "D adjustment in cents.")
		METASOUND_PARAM(TuningCents3, "Cents 3", "D# / Eb adjustment in cents.")
		METASOUND_PARAM(TuningCents4, "Cents 4", "E adjustment in cents.")
		METASOUND_PARAM(TuningCents5, "Cents 5", "F adjustment in cents.")
		METASOUND_PARAM(TuningCents6, "Cents 6", "F# / Gb adjustment in cents.")
		METASOUND_PARAM(TuningCents7, "Cents 7", "G adjustment in cents.")
		METASOUND_PARAM(TuningCents8, "Cents 8", "G# / Ab adjustment in cents.")
		METASOUND_PARAM(TuningCents9, "Cents 9", "A adjustment in cents.")
		METASOUND_PARAM(TuningCents10, "Cents 10", "A# / Bb adjustment in cents.")
		METASOUND_PARAM(TuningCents11, "Cents 11", "B adjustment in cents.")
		METASOUND_PARAM(OutputTime, "Time", "Period of the corresponding pitch frequency in seconds.")
	}

	namespace MidiNoteToTimePrivate
	{
		class FMidiNoteToTimeOperatorData final : public TOperatorData<FMidiNoteToTimeOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FMidiNoteToTimeOperatorData(const EMetaSoundMidiNoteToTimeTuningMode InTuningMode)
				: TuningMode(InTuningMode)
			{
			}

			EMetaSoundMidiNoteToTimeTuningMode TuningMode;
		};

		const FLazyName FMidiNoteToTimeOperatorData::OperatorDataTypeName = TEXT("FMidiNoteToTimeOperatorData");

		void AddTuningCentsInputs(FInputVertexInterface& InInput)
		{
			using namespace MidiNoteToTimeVertexNames;
			InInput.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(TuningCents0), 0.0f));
			InInput.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(TuningCents1), 0.0f));
			InInput.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(TuningCents2), 0.0f));
			InInput.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(TuningCents3), 0.0f));
			InInput.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(TuningCents4), 0.0f));
			InInput.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(TuningCents5), 0.0f));
			InInput.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(TuningCents6), 0.0f));
			InInput.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(TuningCents7), 0.0f));
			InInput.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(TuningCents8), 0.0f));
			InInput.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(TuningCents9), 0.0f));
			InInput.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(TuningCents10), 0.0f));
			InInput.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(TuningCents11), 0.0f));
		}

		FVertexInterface GetVertexInterface(const EMetaSoundMidiNoteToTimeTuningMode InTuningMode)
		{
			using namespace MidiNoteToTimeVertexNames;

			FInputVertexInterface InputInterface(
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(MidiNote), 69.0f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(ReferenceFrequency), 440.0f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(ReferencePitch), 69.0f));

			switch (InTuningMode)
			{
			case EMetaSoundMidiNoteToTimeTuningMode::EDO:
				InputInterface.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Divisions), 12));
				break;
			case EMetaSoundMidiNoteToTimeTuningMode::TuningCents:
				AddTuningCentsInputs(InputInterface);
				break;
			default:
				break;
			}

			FOutputVertexInterface OutputInterface(
				TOutputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTime)));

			return FVertexInterface(MoveTemp(InputInterface), MoveTemp(OutputInterface));
		}

		int32 GetWrappedSemitone(const float InPitch)
		{
			const int32 Base = FMath::FloorToInt(InPitch);
			const int32 Wrapped = Base % 12;
			return Wrapped < 0 ? Wrapped + 12 : Wrapped;
		}

		float GetTuningCentsForPitch(const float InPitch, const float InCents[12])
		{
			const int32 Index0 = GetWrappedSemitone(InPitch);
			const int32 Index1 = (Index0 + 1) % 12;
			const float Whole = FMath::FloorToFloat(InPitch);
			const float Fraction = InPitch - Whole;
			return FMath::Lerp(InCents[Index0], InCents[Index1], Fraction);
		}
	}

	class FMidiNoteToTimeOperator : public TExecutableOperator<FMidiNoteToTimeOperator>
	{
	public:
		using FMidiNoteToTimeOperatorData = MidiNoteToTimePrivate::FMidiNoteToTimeOperatorData;

		FMidiNoteToTimeOperator(
			const TSharedPtr<const FMidiNoteToTimeOperatorData>& InOperatorData,
			const FFloatReadRef& InMidiNote,
			const FFloatReadRef& InReferenceFrequency,
			const FFloatReadRef& InReferencePitch,
			const FInt32ReadRef& InDivisions,
			const FFloatReadRef& InTuningCents0,
			const FFloatReadRef& InTuningCents1,
			const FFloatReadRef& InTuningCents2,
			const FFloatReadRef& InTuningCents3,
			const FFloatReadRef& InTuningCents4,
			const FFloatReadRef& InTuningCents5,
			const FFloatReadRef& InTuningCents6,
			const FFloatReadRef& InTuningCents7,
			const FFloatReadRef& InTuningCents8,
			const FFloatReadRef& InTuningCents9,
			const FFloatReadRef& InTuningCents10,
			const FFloatReadRef& InTuningCents11)
			: OperatorData(InOperatorData)
			, MidiNote(InMidiNote)
			, ReferenceFrequency(InReferenceFrequency)
			, ReferencePitch(InReferencePitch)
			, Divisions(InDivisions)
			, TuningCents0(InTuningCents0)
			, TuningCents1(InTuningCents1)
			, TuningCents2(InTuningCents2)
			, TuningCents3(InTuningCents3)
			, TuningCents4(InTuningCents4)
			, TuningCents5(InTuningCents5)
			, TuningCents6(InTuningCents6)
			, TuningCents7(InTuningCents7)
			, TuningCents8(InTuningCents8)
			, TuningCents9(InTuningCents9)
			, TuningCents10(InTuningCents10)
			, TuningCents11(InTuningCents11)
			, OutputTime(TDataWriteReference<FTime>::CreateNew(FTime::FromSeconds(1.0 / 440.0)))
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = MidiNoteToTimePrivate::GetVertexInterface(EMetaSoundMidiNoteToTimeTuningMode::EqualTemperament12);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("UE"), TEXT("MidiNoteToTime"), TEXT("Float") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 1;
			Metadata.DisplayName = LOCTEXT("DisplayName", "MIDI Note to Time");
			Metadata.Description = LOCTEXT("Description", "Converts note values to period time with selectable 12-TET, EDO, or per-note tuning cents mapping.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = PluginNodeMissingPrompt;
			Metadata.DefaultInterface = DeclareVertexInterface();
			Metadata.CategoryHierarchy = {
				METASOUND_LOCTEXT("Custom", "Branches"),
				METASOUND_LOCTEXT("CustomSub", "Pitch")
			};
			METASOUND_BRANCHES_APPLY_NODE_STYLE(Metadata);
			return Metadata;
		}

		METASOUND_DISABLE_LEGACY_IO()

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace MidiNoteToTimeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(MidiNote), MidiNote);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ReferenceFrequency), ReferenceFrequency);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ReferencePitch), ReferencePitch);

			switch (OperatorData->TuningMode)
			{
			case EMetaSoundMidiNoteToTimeTuningMode::EDO:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Divisions), Divisions);
				break;
			case EMetaSoundMidiNoteToTimeTuningMode::TuningCents:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TuningCents0), TuningCents0);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TuningCents1), TuningCents1);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TuningCents2), TuningCents2);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TuningCents3), TuningCents3);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TuningCents4), TuningCents4);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TuningCents5), TuningCents5);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TuningCents6), TuningCents6);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TuningCents7), TuningCents7);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TuningCents8), TuningCents8);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TuningCents9), TuningCents9);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TuningCents10), TuningCents10);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TuningCents11), TuningCents11);
				break;
			default:
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace MidiNoteToTimeVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputTime), OutputTime);
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
		{
			using namespace MidiNoteToTimeVertexNames;
			using FMidiNoteToTimeOperatorData = MidiNoteToTimePrivate::FMidiNoteToTimeOperatorData;

			const FMidiNoteToTimeOperatorData* ConfigData = CastOperatorData<const FMidiNoteToTimeOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FMidiNoteToTimeOperatorData>& OperatorDataSharedPtr =
				StaticCastSharedPtr<const FMidiNoteToTimeOperatorData>(InParams.Node.GetOperatorData());

			auto InMidiNote = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(MidiNote), InParams.OperatorSettings);
			auto InReferenceFrequency = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(ReferenceFrequency), InParams.OperatorSettings);
			auto InReferencePitch = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(ReferencePitch), InParams.OperatorSettings);
			auto InDivisions = InParams.InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(Divisions), InParams.OperatorSettings);
			auto InTuningCents0 = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(TuningCents0), InParams.OperatorSettings);
			auto InTuningCents1 = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(TuningCents1), InParams.OperatorSettings);
			auto InTuningCents2 = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(TuningCents2), InParams.OperatorSettings);
			auto InTuningCents3 = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(TuningCents3), InParams.OperatorSettings);
			auto InTuningCents4 = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(TuningCents4), InParams.OperatorSettings);
			auto InTuningCents5 = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(TuningCents5), InParams.OperatorSettings);
			auto InTuningCents6 = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(TuningCents6), InParams.OperatorSettings);
			auto InTuningCents7 = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(TuningCents7), InParams.OperatorSettings);
			auto InTuningCents8 = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(TuningCents8), InParams.OperatorSettings);
			auto InTuningCents9 = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(TuningCents9), InParams.OperatorSettings);
			auto InTuningCents10 = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(TuningCents10), InParams.OperatorSettings);
			auto InTuningCents11 = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(TuningCents11), InParams.OperatorSettings);

			return MakeUnique<FMidiNoteToTimeOperator>(
				OperatorDataSharedPtr,
				InMidiNote,
				InReferenceFrequency,
				InReferencePitch,
				InDivisions,
				InTuningCents0,
				InTuningCents1,
				InTuningCents2,
				InTuningCents3,
				InTuningCents4,
				InTuningCents5,
				InTuningCents6,
				InTuningCents7,
				InTuningCents8,
				InTuningCents9,
				InTuningCents10,
				InTuningCents11);
		}

		void Execute()
		{
			const float RefFreq = FMath::Max(KINDA_SMALL_NUMBER, *ReferenceFrequency);
			float Frequency = RefFreq;

			switch (OperatorData->TuningMode)
			{
			case EMetaSoundMidiNoteToTimeTuningMode::EDO:
			{
				const int32 SafeDivisions = FMath::Max(1, *Divisions);
				Frequency = RefFreq * FMath::Pow(2.0f, (*MidiNote - *ReferencePitch) / static_cast<float>(SafeDivisions));
				break;
			}
			case EMetaSoundMidiNoteToTimeTuningMode::TuningCents:
			{
				const float Cents[12] = {
					*TuningCents0, *TuningCents1, *TuningCents2, *TuningCents3, *TuningCents4, *TuningCents5,
					*TuningCents6, *TuningCents7, *TuningCents8, *TuningCents9, *TuningCents10, *TuningCents11
				};
				const float MidiAdjusted = *MidiNote + MidiNoteToTimePrivate::GetTuningCentsForPitch(*MidiNote, Cents) / 100.0f;
				const float RefAdjusted = *ReferencePitch + MidiNoteToTimePrivate::GetTuningCentsForPitch(*ReferencePitch, Cents) / 100.0f;
				Frequency = RefFreq * FMath::Pow(2.0f, (MidiAdjusted - RefAdjusted) / 12.0f);
				break;
			}
			default:
				Frequency = RefFreq * FMath::Pow(2.0f, (*MidiNote - *ReferencePitch) / 12.0f);
				break;
			}

			const double Period = 1.0 / FMath::Max(static_cast<double>(KINDA_SMALL_NUMBER), static_cast<double>(Frequency));
			*OutputTime = FTime::FromSeconds(Period);
		}

	private:
		TSharedPtr<const FMidiNoteToTimeOperatorData> OperatorData;
		FFloatReadRef MidiNote;
		FFloatReadRef ReferenceFrequency;
		FFloatReadRef ReferencePitch;
		FInt32ReadRef Divisions;
		FFloatReadRef TuningCents0;
		FFloatReadRef TuningCents1;
		FFloatReadRef TuningCents2;
		FFloatReadRef TuningCents3;
		FFloatReadRef TuningCents4;
		FFloatReadRef TuningCents5;
		FFloatReadRef TuningCents6;
		FFloatReadRef TuningCents7;
		FFloatReadRef TuningCents8;
		FFloatReadRef TuningCents9;
		FFloatReadRef TuningCents10;
		FFloatReadRef TuningCents11;
		TDataWriteReference<FTime> OutputTime;
	};

	using FMidiNoteToTimeNode = TNodeFacade<FMidiNoteToTimeOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FMidiNoteToTimeNode, FMetaSoundMidiNoteToTimeNodeConfiguration);
}

FMetaSoundMidiNoteToTimeNodeConfiguration::FMetaSoundMidiNoteToTimeNodeConfiguration()
	: OperatorData(MakeShared<Metasound::MidiNoteToTimePrivate::FMidiNoteToTimeOperatorData>(TuningMode))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundMidiNoteToTimeNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::MidiNoteToTimePrivate::GetVertexInterface(TuningMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundMidiNoteToTimeNodeConfiguration::GetOperatorData() const
{
	OperatorData->TuningMode = TuningMode;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE