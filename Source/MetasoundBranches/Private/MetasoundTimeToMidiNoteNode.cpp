// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundTimeToMidiNoteNode.h"

#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTime.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_TimeToMidiNoteNode"

namespace Metasound
{
	namespace TimeToMidiNoteVertexNames
	{
		METASOUND_PARAM(InputTime, "Time", "Time period to convert to note number.")
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
		METASOUND_PARAM(MidiNote, "MIDI Note", "MIDI note number (float, unquantized) matching the input time.")
	}

	namespace TimeToMidiNotePrivate
	{
		class FTimeToMidiNoteOperatorData final : public TOperatorData<FTimeToMidiNoteOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FTimeToMidiNoteOperatorData(const EMetaSoundTimeToMidiNoteTuningMode InTuningMode)
				: TuningMode(InTuningMode)
			{
			}

			EMetaSoundTimeToMidiNoteTuningMode TuningMode;
		};

		const FLazyName FTimeToMidiNoteOperatorData::OperatorDataTypeName = TEXT("FTimeToMidiNoteOperatorData");

		void AddTuningCentsInputs(FInputVertexInterface& InInput)
		{
			using namespace TimeToMidiNoteVertexNames;
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

		FVertexInterface GetVertexInterface(const EMetaSoundTimeToMidiNoteTuningMode InTuningMode)
		{
			using namespace TimeToMidiNoteVertexNames;

			FInputVertexInterface InputInterface(
				TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTime)),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(ReferenceFrequency), 440.0f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(ReferencePitch), 69.0f));

			switch (InTuningMode)
			{
			case EMetaSoundTimeToMidiNoteTuningMode::EDO:
				InputInterface.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Divisions), 12));
				break;
			case EMetaSoundTimeToMidiNoteTuningMode::TuningCents:
				AddTuningCentsInputs(InputInterface);
				break;
			default:
				break;
			}

			FOutputVertexInterface OutputInterface(
				TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(MidiNote)));

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

		float GetFrequencyForPitch(const float InPitch, const float InReferencePitch, const float InReferenceFrequency, const float InCents[12])
		{
			const float PitchAdjusted = InPitch + GetTuningCentsForPitch(InPitch, InCents) / 100.0f;
			const float RefAdjusted = InReferencePitch + GetTuningCentsForPitch(InReferencePitch, InCents) / 100.0f;
			return InReferenceFrequency * FMath::Pow(2.0f, (PitchAdjusted - RefAdjusted) / 12.0f);
		}
	}

	class FTimeToMidiNoteOperator : public TExecutableOperator<FTimeToMidiNoteOperator>
	{
	public:
		using FTimeToMidiNoteOperatorData = TimeToMidiNotePrivate::FTimeToMidiNoteOperatorData;

		FTimeToMidiNoteOperator(
			const TSharedPtr<const FTimeToMidiNoteOperatorData>& InOperatorData,
			const TDataReadReference<FTime>& InTime,
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
			, InputTime(InTime)
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
			, MidiNote(TDataWriteReference<float>::CreateNew(69.0f))
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = TimeToMidiNotePrivate::GetVertexInterface(EMetaSoundTimeToMidiNoteTuningMode::EqualTemperament12);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("UE"), TEXT("TimeToMidiNote"), TEXT("Float") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 1;
			Metadata.DisplayName = LOCTEXT("DisplayName", "Time to MIDI Note");
			Metadata.Description = LOCTEXT("Description", "Converts period time to note values with selectable 12-TET, EDO, or per-note tuning cents mapping.");
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
			using namespace TimeToMidiNoteVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTime), InputTime);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ReferenceFrequency), ReferenceFrequency);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ReferencePitch), ReferencePitch);

			switch (OperatorData->TuningMode)
			{
			case EMetaSoundTimeToMidiNoteTuningMode::EDO:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Divisions), Divisions);
				break;
			case EMetaSoundTimeToMidiNoteTuningMode::TuningCents:
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
			using namespace TimeToMidiNoteVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(MidiNote), MidiNote);
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
		{
			using namespace TimeToMidiNoteVertexNames;
			using FTimeToMidiNoteOperatorData = TimeToMidiNotePrivate::FTimeToMidiNoteOperatorData;

			const FTimeToMidiNoteOperatorData* ConfigData = CastOperatorData<const FTimeToMidiNoteOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FTimeToMidiNoteOperatorData>& OperatorDataSharedPtr =
				StaticCastSharedPtr<const FTimeToMidiNoteOperatorData>(InParams.Node.GetOperatorData());

			auto InTime = InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(InputTime), InParams.OperatorSettings);
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

			return MakeUnique<FTimeToMidiNoteOperator>(
				OperatorDataSharedPtr,
				InTime,
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
			const double Seconds = InputTime->GetSeconds();
			const double Frequency = 1.0 / FMath::Max(static_cast<double>(KINDA_SMALL_NUMBER), Seconds);
			const float RefFreq = FMath::Max(KINDA_SMALL_NUMBER, *ReferenceFrequency);

			switch (OperatorData->TuningMode)
			{
			case EMetaSoundTimeToMidiNoteTuningMode::EDO:
			{
				const int32 SafeDivisions = FMath::Max(1, *Divisions);
				*MidiNote = *ReferencePitch + static_cast<float>(SafeDivisions) * static_cast<float>(FMath::Log2(Frequency / RefFreq));
				break;
			}
			case EMetaSoundTimeToMidiNoteTuningMode::TuningCents:
			{
				const float Cents[12] = {
					*TuningCents0, *TuningCents1, *TuningCents2, *TuningCents3, *TuningCents4, *TuningCents5,
					*TuningCents6, *TuningCents7, *TuningCents8, *TuningCents9, *TuningCents10, *TuningCents11
				};

				float Estimate = *ReferencePitch + 12.0f * static_cast<float>(FMath::Log2(Frequency / RefFreq));
				for (int32 Iteration = 0; Iteration < 8; ++Iteration)
				{
					const float CurrentFrequency = TimeToMidiNotePrivate::GetFrequencyForPitch(Estimate, *ReferencePitch, RefFreq, Cents);
					const float Error = CurrentFrequency - static_cast<float>(Frequency);
					if (FMath::Abs(Error) <= 1.0e-6f)
					{
						break;
					}

					const float Epsilon = 0.001f;
					const float FrequencyPlus = TimeToMidiNotePrivate::GetFrequencyForPitch(Estimate + Epsilon, *ReferencePitch, RefFreq, Cents);
					const float FrequencyMinus = TimeToMidiNotePrivate::GetFrequencyForPitch(Estimate - Epsilon, *ReferencePitch, RefFreq, Cents);
					const float Derivative = (FrequencyPlus - FrequencyMinus) / (2.0f * Epsilon);
					if (FMath::Abs(Derivative) <= 1.0e-6f)
					{
						break;
					}

					Estimate -= Error / Derivative;
				}

				*MidiNote = Estimate;
				break;
			}
			default:
				*MidiNote = *ReferencePitch + 12.0f * static_cast<float>(FMath::Log2(Frequency / RefFreq));
				break;
			}
		}

	private:
		TSharedPtr<const FTimeToMidiNoteOperatorData> OperatorData;
		TDataReadReference<FTime> InputTime;
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
		TDataWriteReference<float> MidiNote;
	};

	using FTimeToMidiNoteNode = TNodeFacade<FTimeToMidiNoteOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FTimeToMidiNoteNode, FMetaSoundTimeToMidiNoteNodeConfiguration);
}

FMetaSoundTimeToMidiNoteNodeConfiguration::FMetaSoundTimeToMidiNoteNodeConfiguration()
	: OperatorData(MakeShared<Metasound::TimeToMidiNotePrivate::FTimeToMidiNoteOperatorData>(TuningMode))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundTimeToMidiNoteNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::TimeToMidiNotePrivate::GetVertexInterface(TuningMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundTimeToMidiNoteNodeConfiguration::GetOperatorData() const
{
	OperatorData->TuningMode = TuningMode;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE