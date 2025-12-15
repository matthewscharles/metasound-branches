#include "MetasoundBranches/Public/CatFormatOptionsHelper.h"

#include "TypeFamily/ChannelTypeFamily.h"
#include "Algo/Transform.h"

TArray<FPropertyTextFName>
UCatFormatOptionsHelper::GetCatFormatOptions()
{
    const TArray<const Audio::FChannelTypeFamily*> AllFormats =
        Audio::GetChannelRegistry().GetAllChannelFormats();

    TArray<FPropertyTextFName> Options;
    Options.Reserve(AllFormats.Num());

    Algo::Transform(AllFormats, Options,
        [](const Audio::FChannelTypeFamily* Format)
        {
            return FPropertyTextFName{
                .ValueString = Format->GetName(),
                .DisplayName = FText::FromString(Format->GetFriendlyName())
            };
        });

    return Options;
}