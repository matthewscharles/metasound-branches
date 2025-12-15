// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundCatPhasedLFONode.h"

#include "MetasoundChannelAgnosticType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include "TypeFamily/ChannelTypeFamily.h"

#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_CatPhasedLFO"

namespace Metasound
{
	namespace CatPhasedLFOPrivate
	{
		METASOUND_PARAM(InputFrequency, "Frequency", "LFO frequency in Hz (shared across channels).");
		METASOUND_PARAM(InputMin,       "Min",       "Minimum output value.");
		METASOUND_PARAM(InputMax,       "Max",       "Maximum output value.");
		METASOUND_PARAM(InputReset,     "Reset",     "Reset phase to zero.");
		METASOUND_PARAM(OutputCat,      "Out",       "Channel-agnostic phased LFO output.");
		METASOUND_PARAM(OutputReset,    "ResetOut",  "Reset trigger passthrough.");

		class FCatPhasedLFOOperatorData final : public TOperatorData<FCatPhasedLFOOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCatPhasedLFOOperatorData(const FName& InToTypeName)
				: ToTypeName(InToTypeName)
			{}

			const FName& GetToType() const { return ToTypeName; }

		private:
			FName ToTypeName;
		};

		const FLazyName FCatPhasedLFOOperatorData::OperatorDataTypeName =
			TEXT("FCatPhasedLFOOperatorData");
	}

	class FCatPhasedLFOOperator final : public TExecutableOperator<FCatPhasedLFOOperator>
	{
	public:
		FCatPhasedLFOOperator(
			const FBuildOperatorParams& InParams,
			FFloatReadRef&& InFrequency,
			FFloatReadRef&& InMin,
			FFloatReadRef&& InMax,
			FTriggerReadRef&& InReset,
			const FName InConcreteTypeName)
			: Settings(InParams.OperatorSettings)
			, Frequency(MoveTemp(InFrequency))
			, Min(MoveTemp(InMin))
			, Max(MoveTemp(InMax))
			, ResetIn(MoveTemp(InReset))
			, ResetOut(FTriggerWriteRef::CreateNew(Settings))
			, Output(FChannelAgnosticTypeWriteRef::CreateNew(Settings, InConcreteTypeName))
		{
		}

		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace CatPhasedLFOPrivate;

			auto CreateDefaultInterface = []() -> FVertexInterface
			{
				FInputVertexInterface Inputs;
				Inputs.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFrequency), 1.0f));
				Inputs.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMin), 0.0f));
				Inputs.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMax), 1.0f));
				Inputs.Add(TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReset)));

				FOutputVertexInterface Outputs;
				Outputs.Add(TOutputDataVertex<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputCat)));
				Outputs.Add(TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputReset)));

				return FVertexInterface(MoveTemp(Inputs), MoveTemp(Outputs));
			};

			static const FVertexInterface DefaultInterface = CreateDefaultInterface();
			return DefaultInterface;
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults&)
		{
			using namespace CatPhasedLFOPrivate;

			const FCatPhasedLFOOperatorData* OpData =
				CastOperatorData<const FCatPhasedLFOOperatorData>(InParams.Node.GetOperatorData().Get());

			if (!OpData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* Concrete =
				Audio::GetChannelRegistry().FindConcreteChannel(OpData->GetToType());

			if (!Concrete)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			FFloatReadRef Frequency =
				InputData.GetOrCreateDefaultDataReadReference<float>(
					METASOUND_GET_PARAM_NAME(InputFrequency), InParams.OperatorSettings);

			FFloatReadRef Min =
				InputData.GetOrCreateDefaultDataReadReference<float>(
					METASOUND_GET_PARAM_NAME(InputMin), InParams.OperatorSettings);

			FFloatReadRef Max =
				InputData.GetOrCreateDefaultDataReadReference<float>(
					METASOUND_GET_PARAM_NAME(InputMax), InParams.OperatorSettings);

			FTriggerReadRef Reset =
				InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
					METASOUND_GET_PARAM_NAME(InputReset), InParams.OperatorSettings);

			return MakeUnique<FCatPhasedLFOOperator>(
				InParams,
				MoveTemp(Frequency),
				MoveTemp(Min),
				MoveTemp(Max),
				MoveTemp(Reset),
				Concrete->GetName()
			);
		}

		METASOUND_DISABLE_LEGACY_IO()

		void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CatPhasedLFOPrivate;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputFrequency), Frequency);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputMin), Min);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputMax), Max);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputReset), ResetIn);
		}

		void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CatPhasedLFOPrivate;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputCat), Output);
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputReset), ResetOut);
		}

		void Reset(const IOperator::FResetParams&)
		{
			BasePhase01 = 0.0f;
			ResetOut->Reset();
		}

		void Execute()
		{
			ResetOut->AdvanceBlock();

			// Reset phase on trigger (and pass it through).
			if (ResetIn->IsTriggeredInBlock())
			{
				BasePhase01 = 0.0f;
				ResetOut->TriggerFrame(0);
			}

			const float SampleRate = Settings.GetSampleRate();
			const int32 NumFrames = Settings.GetNumFramesPerBlock();

			const float FreqHz = FMath::Max(0.0f, *Frequency);
			const float PhaseInc01 = (SampleRate > 0.0f) ? (FreqHz / SampleRate) : 0.0f;

			const float MinVal = *Min;
			const float MaxVal = *Max;
			const float Range = (MaxVal - MinVal);

			const int32 NumChannels = Output->NumChannels();

			for (int32 i = 0; i < NumFrames; ++i)
			{
				for (int32 ch = 0; ch < NumChannels; ++ch)
				{
					// Offset spread is exactly ch / NumChannels
					const float Offset01 = (NumChannels > 0)
						? (static_cast<float>(ch) / static_cast<float>(NumChannels))
						: 0.0f;

					const float Phase01 = BasePhase01 + Offset01;
					const float Wrapped01 = Phase01 - FMath::FloorToFloat(Phase01);

					const float Sine = FMath::Sin(Wrapped01 * UE_TWO_PI);

					// Map [-1,1] -> [0,1] then scale to [Min,Max].
					Output->GetChannel(ch)[i] = MinVal + (0.5f + 0.5f * Sine) * Range;
				}

				BasePhase01 += PhaseInc01;
				BasePhase01 -= FMath::FloorToFloat(BasePhase01);
			}
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			return FNodeClassMetadata
			{
				FNodeClassName{ TEXT("UE"), TEXT("CAT Phased LFO"), TEXT("Control") },
				1, // Major
				0, // Minor
				METASOUND_LOCTEXT("CatPhasedLFODisplayName", "CAT Phased LFO"),
				METASOUND_LOCTEXT("CatPhasedLFODesc", "Generates a channel-agnostic sine LFO with phase offset distributed as ch / NumChannels."),
				TEXT("Charles Matthews"),
				PluginNodeMissingPrompt,
				GetDefaultInterface(),
				{
					METASOUND_LOCTEXT("BranchesCategory", "Branches"),
					METASOUND_LOCTEXT("ModulationCategory", "Modulation")
				},
				{
					METASOUND_LOCTEXT("Keyword_LFO", "LFO"),
					METASOUND_LOCTEXT("Keyword_CAT", "CAT"),
					METASOUND_LOCTEXT("Keyword_Phase", "Phase"),
					METASOUND_LOCTEXT("Keyword_Modulation", "Modulation")
				}
			};
		}

	private:
		FOperatorSettings Settings;

		FFloatReadRef Frequency;
		FFloatReadRef Min;
		FFloatReadRef Max;
		FTriggerReadRef ResetIn;

		FTriggerWriteRef ResetOut;
		FChannelAgnosticTypeWriteRef Output;

		float BasePhase01 = 0.0f;
	};

	using FCatPhasedLFONode = TNodeFacade<FCatPhasedLFOOperator>;

	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCatPhasedLFONode, FMetaSoundCatPhasedLFONodeConfiguration);
} // namespace Metasound

TInstancedStruct<FMetasoundFrontendClassInterface>
FMetaSoundCatPhasedLFONodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass&) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::FCatPhasedLFOOperator::GetDefaultInterface()));
}

TSharedPtr<const Metasound::IOperatorData>
FMetaSoundCatPhasedLFONodeConfiguration::GetOperatorData() const
{
	return MakeShared<Metasound::CatPhasedLFOPrivate::FCatPhasedLFOOperatorData>(ToType);
}

#undef LOCTEXT_NAMESPACE