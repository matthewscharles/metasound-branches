// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundCATMapRangeNode.h"

#include "MetasoundAudioBuffer.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundChannelAgnosticType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundVertex.h"
#include "Math/UnrealMathUtility.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATMapRangeNode"

namespace Metasound
{
	namespace CATMapRangeVertexNames
	{
		METASOUND_PARAM(Input, "Input", "CAT input signal to map.")
		METASOUND_PARAM(InRangeA, "In Range A", "Input range A.")
		METASOUND_PARAM(InRangeB, "In Range B", "Input range B.")
		METASOUND_PARAM(OutRangeA, "Out Range A", "Output range A.")
		METASOUND_PARAM(OutRangeB, "Out Range B", "Output range B.")
		METASOUND_PARAM(Clamp, "Clamp", "Whether or not to clamp the mapped output.")
		METASOUND_PARAM(Output, "Output", "Mapped CAT output.")
	}

	namespace CATMapRangePrivate
	{
		class FCATMapRangeOperatorData final : public TOperatorData<FCATMapRangeOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATMapRangeOperatorData(const FName& InCatAudioTypeName, const EMetaSoundCATMapRangeEndpointMode InEndpointMode)
				: CatAudioTypeName(InCatAudioTypeName)
				, EndpointMode(InEndpointMode)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATMapRangeEndpointMode EndpointMode;
		};

		const FLazyName FCATMapRangeOperatorData::OperatorDataTypeName = TEXT("FCATMapRangeOperatorData");

		FVertexInterface GetVertexInterface(const FName& InCatFormat, const EMetaSoundCATMapRangeEndpointMode InEndpointMode)
		{
			using namespace CATMapRangeVertexNames;

			FInputVertexInterface InputInterface;
			InputInterface.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(Input), InCatFormat, METASOUND_GET_PARAM_METADATA(Input), EVertexAccessType::Reference));

			switch (InEndpointMode)
			{
			case EMetaSoundCATMapRangeEndpointMode::CAT:
				InputInterface.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(InRangeA), InCatFormat, METASOUND_GET_PARAM_METADATA(InRangeA), EVertexAccessType::Reference));
				InputInterface.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(InRangeB), InCatFormat, METASOUND_GET_PARAM_METADATA(InRangeB), EVertexAccessType::Reference));
				InputInterface.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(OutRangeA), InCatFormat, METASOUND_GET_PARAM_METADATA(OutRangeA), EVertexAccessType::Reference));
				InputInterface.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(OutRangeB), InCatFormat, METASOUND_GET_PARAM_METADATA(OutRangeB), EVertexAccessType::Reference));
				break;
			case EMetaSoundCATMapRangeEndpointMode::MonoAudio:
				InputInterface.Add(TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InRangeA)));
				InputInterface.Add(TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InRangeB)));
				InputInterface.Add(TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutRangeA)));
				InputInterface.Add(TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutRangeB)));
				break;
			case EMetaSoundCATMapRangeEndpointMode::Float:
				InputInterface.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InRangeA), 0.0f));
				InputInterface.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InRangeB), 1.0f));
				InputInterface.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutRangeA), 0.0f));
				InputInterface.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutRangeB), 1.0f));
				break;
			case EMetaSoundCATMapRangeEndpointMode::FloatArray:
				InputInterface.Add(TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(InRangeA)));
				InputInterface.Add(TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(InRangeB)));
				InputInterface.Add(TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutRangeA)));
				InputInterface.Add(TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutRangeB)));
				break;
			default:
				checkNoEntry();
				break;
			}

			InputInterface.Add(TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(Clamp), true));

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(InputInterface), MoveTemp(OutputInterface) };
		}
	}

	class FCATMapRangeOperator final : public TExecutableOperator<FCATMapRangeOperator>
	{
	public:
		using FCATMapRangeOperatorData = CATMapRangePrivate::FCATMapRangeOperatorData;

		FCATMapRangeOperator(
			const TSharedPtr<const FCATMapRangeOperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FChannelAgnosticTypeReadRef&& InInput,
			FChannelAgnosticTypeReadRef&& InInRangeACAT,
			FChannelAgnosticTypeReadRef&& InInRangeBCAT,
			FChannelAgnosticTypeReadRef&& InOutRangeACAT,
			FChannelAgnosticTypeReadRef&& InOutRangeBCAT,
			FAudioBufferReadRef&& InInRangeAMono,
			FAudioBufferReadRef&& InInRangeBMono,
			FAudioBufferReadRef&& InOutRangeAMono,
			FAudioBufferReadRef&& InOutRangeBMono,
			FFloatReadRef&& InInRangeAFloat,
			FFloatReadRef&& InInRangeBFloat,
			FFloatReadRef&& InOutRangeAFloat,
			FFloatReadRef&& InOutRangeBFloat,
			TDataReadReference<TArray<float>>&& InInRangeAArray,
			TDataReadReference<TArray<float>>&& InInRangeBArray,
			TDataReadReference<TArray<float>>&& InOutRangeAArray,
			TDataReadReference<TArray<float>>&& InOutRangeBArray,
			FBoolReadRef&& InClamp,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, Input(MoveTemp(InInput))
			, InRangeACAT(MoveTemp(InInRangeACAT))
			, InRangeBCAT(MoveTemp(InInRangeBCAT))
			, OutRangeACAT(MoveTemp(InOutRangeACAT))
			, OutRangeBCAT(MoveTemp(InOutRangeBCAT))
			, InRangeAMono(MoveTemp(InInRangeAMono))
			, InRangeBMono(MoveTemp(InInRangeBMono))
			, OutRangeAMono(MoveTemp(InOutRangeAMono))
			, OutRangeBMono(MoveTemp(InOutRangeBMono))
			, InRangeAFloat(MoveTemp(InInRangeAFloat))
			, InRangeBFloat(MoveTemp(InInRangeBFloat))
			, OutRangeAFloat(MoveTemp(InOutRangeAFloat))
			, OutRangeBFloat(MoveTemp(InOutRangeBFloat))
			, InRangeAArray(MoveTemp(InInRangeAArray))
			, InRangeBArray(MoveTemp(InInRangeBArray))
			, OutRangeAArray(MoveTemp(InOutRangeAArray))
			, OutRangeBArray(MoveTemp(InOutRangeBArray))
			, Clamp(MoveTemp(InClamp))
			, Output(MoveTemp(InOutput))
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATMapRangePrivate::GetVertexInterface(TEXT("Cat:Stereo2Dot0"), EMetaSoundCATMapRangeEndpointMode::Float);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATMapRange"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATMapRangeDisplayName", "CAT Map Range");
			Metadata.Description = LOCTEXT("CATMapRangeDescription", "Map CAT input from one range to another with optional clamp.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATMapRangeMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
			Metadata.CategoryHierarchy = {
				METASOUND_LOCTEXT("Custom", "Branches"),
				METASOUND_LOCTEXT("CustomSub", "CAT")
			};
			Metadata.DefaultInterface = DeclareVertexInterface();
			METASOUND_BRANCHES_APPLY_CAT_NODE_STYLE(Metadata);
			return Metadata;
		}

		METASOUND_DISABLE_LEGACY_IO()

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
		{
			using namespace CATMapRangeVertexNames;

			const FCATMapRangeOperatorData* ConfigData = CastOperatorData<const FCATMapRangeOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATMapRangeOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATMapRangeOperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InSignal = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Input), InParams.OperatorSettings);

			FChannelAgnosticTypeReadRef InRangeACATRef = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FChannelAgnosticTypeReadRef InRangeBCATRef = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FChannelAgnosticTypeReadRef OutRangeACATRef = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FChannelAgnosticTypeReadRef OutRangeBCATRef = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			FAudioBufferReadRef InRangeAMonoRef = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FAudioBufferReadRef InRangeBMonoRef = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FAudioBufferReadRef OutRangeAMonoRef = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FAudioBufferReadRef OutRangeBMonoRef = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);

			FFloatReadRef InRangeAFloatRef = TDataReadReference<float>::CreateNew(0.0f);
			FFloatReadRef InRangeBFloatRef = TDataReadReference<float>::CreateNew(1.0f);
			FFloatReadRef OutRangeAFloatRef = TDataReadReference<float>::CreateNew(0.0f);
			FFloatReadRef OutRangeBFloatRef = TDataReadReference<float>::CreateNew(1.0f);

			TDataReadReference<TArray<float>> InRangeAArrayRef = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});
			TDataReadReference<TArray<float>> InRangeBArrayRef = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});
			TDataReadReference<TArray<float>> OutRangeAArrayRef = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});
			TDataReadReference<TArray<float>> OutRangeBArrayRef = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});

			FBoolReadRef InClampRef = InParams.InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(Clamp), InParams.OperatorSettings);

			switch (ConfigData->EndpointMode)
			{
			case EMetaSoundCATMapRangeEndpointMode::CAT:
				InRangeACATRef = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(InRangeA), InParams.OperatorSettings);
				InRangeBCATRef = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(InRangeB), InParams.OperatorSettings);
				OutRangeACATRef = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(OutRangeA), InParams.OperatorSettings);
				OutRangeBCATRef = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(OutRangeB), InParams.OperatorSettings);
				break;
			case EMetaSoundCATMapRangeEndpointMode::MonoAudio:
				InRangeAMonoRef = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InRangeA), InParams.OperatorSettings);
				InRangeBMonoRef = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InRangeB), InParams.OperatorSettings);
				OutRangeAMonoRef = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(OutRangeA), InParams.OperatorSettings);
				OutRangeBMonoRef = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(OutRangeB), InParams.OperatorSettings);
				break;
			case EMetaSoundCATMapRangeEndpointMode::Float:
				InRangeAFloatRef = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InRangeA), InParams.OperatorSettings);
				InRangeBFloatRef = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InRangeB), InParams.OperatorSettings);
				OutRangeAFloatRef = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(OutRangeA), InParams.OperatorSettings);
				OutRangeBFloatRef = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(OutRangeB), InParams.OperatorSettings);
				break;
			case EMetaSoundCATMapRangeEndpointMode::FloatArray:
				InRangeAArrayRef = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(InRangeA), InParams.OperatorSettings);
				InRangeBArrayRef = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(InRangeB), InParams.OperatorSettings);
				OutRangeAArrayRef = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(OutRangeA), InParams.OperatorSettings);
				OutRangeBArrayRef = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(OutRangeB), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FChannelAgnosticTypeWriteRef OutRef = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATMapRangeOperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InSignal),
				MoveTemp(InRangeACATRef),
				MoveTemp(InRangeBCATRef),
				MoveTemp(OutRangeACATRef),
				MoveTemp(OutRangeBCATRef),
				MoveTemp(InRangeAMonoRef),
				MoveTemp(InRangeBMonoRef),
				MoveTemp(OutRangeAMonoRef),
				MoveTemp(OutRangeBMonoRef),
				MoveTemp(InRangeAFloatRef),
				MoveTemp(InRangeBFloatRef),
				MoveTemp(OutRangeAFloatRef),
				MoveTemp(OutRangeBFloatRef),
				MoveTemp(InRangeAArrayRef),
				MoveTemp(InRangeBArrayRef),
				MoveTemp(OutRangeAArrayRef),
				MoveTemp(OutRangeBArrayRef),
				MoveTemp(InClampRef),
				MoveTemp(OutRef));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATMapRangeVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Input), Input);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Clamp), Clamp);

			switch (OperatorData->EndpointMode)
			{
			case EMetaSoundCATMapRangeEndpointMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InRangeA), InRangeACAT);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InRangeB), InRangeBCAT);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutRangeA), OutRangeACAT);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutRangeB), OutRangeBCAT);
				break;
			case EMetaSoundCATMapRangeEndpointMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InRangeA), InRangeAMono);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InRangeB), InRangeBMono);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutRangeA), OutRangeAMono);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutRangeB), OutRangeBMono);
				break;
			case EMetaSoundCATMapRangeEndpointMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InRangeA), InRangeAFloat);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InRangeB), InRangeBFloat);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutRangeA), OutRangeAFloat);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutRangeB), OutRangeBFloat);
				break;
			case EMetaSoundCATMapRangeEndpointMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InRangeA), InRangeAArray);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InRangeB), InRangeBArray);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutRangeA), OutRangeAArray);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutRangeB), OutRangeBArray);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATMapRangeVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(Output), Output);
		}

		void Execute()
		{
			constexpr float KSmallNumber = 1.0e-6f;
			Output->Zero();

			const int32 NumFrames = Settings.GetNumFramesPerBlock();
			const int32 NumOutChannels = Output->NumChannels();
			if (NumOutChannels <= 0 || NumFrames <= 0)
			{
				return;
			}

			const int32 NumInputChannels = FMath::Max(1, Input->NumChannels());
			const bool bClamp = *Clamp;

			auto GetArrayValue = [](const TArray<float>& InValues, const int32 InChannel, const float InDefault) -> float
			{
				if (InValues.Num() == 1)
				{
					return InValues[0];
				}
				if (InValues.Num() > 1)
				{
					return InValues[FMath::Min(InChannel, InValues.Num() - 1)];
				}
				return InDefault;
			};

			switch (OperatorData->EndpointMode)
			{
			case EMetaSoundCATMapRangeEndpointMode::Float:
			{
				const float InA = *InRangeAFloat;
				const float InB = *InRangeBFloat;
				const float OutA = *OutRangeAFloat;
				const float OutB = *OutRangeBFloat;

				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const int32 InChannel = (NumInputChannels == 1) ? 0 : FMath::Min(Channel, NumInputChannels - 1);
					TArrayView<const float> Src = Input->GetChannel(InChannel);
					TArrayView<float> Dst = Output->GetChannel(Channel);
					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						const float Denom = InB - InA;
						float Pct = (FMath::Abs(Denom) > KSmallNumber) ? ((Src[Frame] - InA) / Denom) : 0.0f;
						if (bClamp)
						{
							Pct = FMath::Clamp(Pct, 0.0f, 1.0f);
						}
						Dst[Frame] = OutA + (OutB - OutA) * Pct;
					}
				}
				break;
			}
			case EMetaSoundCATMapRangeEndpointMode::MonoAudio:
			{
				const float* InASrc = InRangeAMono->GetData();
				const float* InBSrc = InRangeBMono->GetData();
				const float* OutASrc = OutRangeAMono->GetData();
				const float* OutBSrc = OutRangeBMono->GetData();

				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const int32 InChannel = (NumInputChannels == 1) ? 0 : FMath::Min(Channel, NumInputChannels - 1);
					TArrayView<const float> Src = Input->GetChannel(InChannel);
					TArrayView<float> Dst = Output->GetChannel(Channel);
					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						const float InA = InASrc[Frame];
						const float InB = InBSrc[Frame];
						const float OutA = OutASrc[Frame];
						const float OutB = OutBSrc[Frame];
						const float Denom = InB - InA;
						float Pct = (FMath::Abs(Denom) > KSmallNumber) ? ((Src[Frame] - InA) / Denom) : 0.0f;
						if (bClamp)
						{
							Pct = FMath::Clamp(Pct, 0.0f, 1.0f);
						}
						Dst[Frame] = OutA + (OutB - OutA) * Pct;
					}
				}
				break;
			}
			case EMetaSoundCATMapRangeEndpointMode::CAT:
			{
				const int32 NumInAChannels = FMath::Max(1, InRangeACAT->NumChannels());
				const int32 NumInBChannels = FMath::Max(1, InRangeBCAT->NumChannels());
				const int32 NumOutAChannels = FMath::Max(1, OutRangeACAT->NumChannels());
				const int32 NumOutBChannels = FMath::Max(1, OutRangeBCAT->NumChannels());

				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const int32 SigChannel = (NumInputChannels == 1) ? 0 : FMath::Min(Channel, NumInputChannels - 1);
					const int32 InAChannel = (NumInAChannels == 1) ? 0 : FMath::Min(Channel, NumInAChannels - 1);
					const int32 InBChannel = (NumInBChannels == 1) ? 0 : FMath::Min(Channel, NumInBChannels - 1);
					const int32 OutAChannel = (NumOutAChannels == 1) ? 0 : FMath::Min(Channel, NumOutAChannels - 1);
					const int32 OutBChannel = (NumOutBChannels == 1) ? 0 : FMath::Min(Channel, NumOutBChannels - 1);

					TArrayView<const float> SigSrc = Input->GetChannel(SigChannel);
					TArrayView<const float> InASrc = InRangeACAT->GetChannel(InAChannel);
					TArrayView<const float> InBSrc = InRangeBCAT->GetChannel(InBChannel);
					TArrayView<const float> OutASrc = OutRangeACAT->GetChannel(OutAChannel);
					TArrayView<const float> OutBSrc = OutRangeBCAT->GetChannel(OutBChannel);
					TArrayView<float> Dst = Output->GetChannel(Channel);

					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						const float InA = InASrc[Frame];
						const float InB = InBSrc[Frame];
						const float OutA = OutASrc[Frame];
						const float OutB = OutBSrc[Frame];
						const float Denom = InB - InA;
						float Pct = (FMath::Abs(Denom) > KSmallNumber) ? ((SigSrc[Frame] - InA) / Denom) : 0.0f;
						if (bClamp)
						{
							Pct = FMath::Clamp(Pct, 0.0f, 1.0f);
						}
						Dst[Frame] = OutA + (OutB - OutA) * Pct;
					}
				}
				break;
			}
			case EMetaSoundCATMapRangeEndpointMode::FloatArray:
			{
				const TArray<float>& InAValues = *InRangeAArray;
				const TArray<float>& InBValues = *InRangeBArray;
				const TArray<float>& OutAValues = *OutRangeAArray;
				const TArray<float>& OutBValues = *OutRangeBArray;

				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const int32 InChannel = (NumInputChannels == 1) ? 0 : FMath::Min(Channel, NumInputChannels - 1);
					const float InA = GetArrayValue(InAValues, Channel, 0.0f);
					const float InB = GetArrayValue(InBValues, Channel, 1.0f);
					const float OutA = GetArrayValue(OutAValues, Channel, 0.0f);
					const float OutB = GetArrayValue(OutBValues, Channel, 1.0f);

					TArrayView<const float> Src = Input->GetChannel(InChannel);
					TArrayView<float> Dst = Output->GetChannel(Channel);
					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						const float Denom = InB - InA;
						float Pct = (FMath::Abs(Denom) > KSmallNumber) ? ((Src[Frame] - InA) / Denom) : 0.0f;
						if (bClamp)
						{
							Pct = FMath::Clamp(Pct, 0.0f, 1.0f);
						}
						Dst[Frame] = OutA + (OutB - OutA) * Pct;
					}
				}
				break;
			}
			default:
				checkNoEntry();
				break;
			}
		}

	private:
		TSharedPtr<const FCATMapRangeOperatorData> OperatorData;
		FOperatorSettings Settings;
		FChannelAgnosticTypeReadRef Input;

		FChannelAgnosticTypeReadRef InRangeACAT;
		FChannelAgnosticTypeReadRef InRangeBCAT;
		FChannelAgnosticTypeReadRef OutRangeACAT;
		FChannelAgnosticTypeReadRef OutRangeBCAT;

		FAudioBufferReadRef InRangeAMono;
		FAudioBufferReadRef InRangeBMono;
		FAudioBufferReadRef OutRangeAMono;
		FAudioBufferReadRef OutRangeBMono;

		FFloatReadRef InRangeAFloat;
		FFloatReadRef InRangeBFloat;
		FFloatReadRef OutRangeAFloat;
		FFloatReadRef OutRangeBFloat;

		TDataReadReference<TArray<float>> InRangeAArray;
		TDataReadReference<TArray<float>> InRangeBArray;
		TDataReadReference<TArray<float>> OutRangeAArray;
		TDataReadReference<TArray<float>> OutRangeBArray;

		FBoolReadRef Clamp;
		FChannelAgnosticTypeWriteRef Output;
	};

	using FCATMapRangeNode = TNodeFacade<FCATMapRangeOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATMapRangeNode, FMetaSoundCATMapRangeNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATMapRangeNodeOptionsHelper::GetSoundFileFormatChannelOptions()
{
	const TArray<TSharedRef<const Audio::FChannelTypeFamily>> AllFormats = Audio::GetChannelRegistry().GetAllChannelFormats();
	TArray<FPropertyTextFName> FormatsOptions;
	for (const TSharedRef<const Audio::FChannelTypeFamily>& Format : AllFormats)
	{
		if (const Audio::FDiscreteChannelTypeFamily* Discrete = Format->Cast<Audio::FDiscreteChannelTypeFamily>())
		{
			const FString TypeName = Discrete->GetName().ToString();
			if (TypeName.StartsWith(TEXT("Cat:")))
			{
				FormatsOptions.Emplace(Format->GetName(), FText::FromString(Format->GetFriendlyName()));
			}
		}
	}
	return FormatsOptions;
}

FMetaSoundCATMapRangeNodeConfiguration::FMetaSoundCATMapRangeNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATMapRangePrivate::FCATMapRangeOperatorData>(CatAudioTypeName, EndpointMode))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATMapRangeNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATMapRangePrivate::GetVertexInterface(CatAudioTypeName, EndpointMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATMapRangeNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->EndpointMode = EndpointMode;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
