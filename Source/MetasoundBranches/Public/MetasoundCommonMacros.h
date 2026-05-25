// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#define METASOUND_BRANCHES_STYLE_SET_NAME TEXT("MetaSoundStyle")
#define METASOUND_BRANCHES_COLOR_KEY_DEFAULT TEXT("MetasoundEditor.Graph.Node.Custom.Branches.Color.Default")
#define METASOUND_BRANCHES_ICON_KEY_DEFAULT TEXT("MetasoundEditor.Graph.Node.Custom.Branches.Icon.Default")

#define METASOUND_BRANCHES_APPLY_NODE_STYLE(MetadataVar)                                          \
    do                                                                                            \
    {                                                                                             \
        (MetadataVar).DisplayStyle.StyleSet = FName(METASOUND_BRANCHES_STYLE_SET_NAME);          \
        (MetadataVar).DisplayStyle.Color = FName(METASOUND_BRANCHES_COLOR_KEY_DEFAULT);          \
        (MetadataVar).DisplayStyle.Icon = FName(METASOUND_BRANCHES_ICON_KEY_DEFAULT);            \
    } while (false)

#define METASOUND_DISABLE_LEGACY_IO()                                     \
    /* Disable legacy GetInputs/GetOutputs — BindInputs used instead */   \
    virtual FDataReferenceCollection GetInputs() const override           \
    {                                                                     \
        checkNoEntry();                                                   \
        return {};                                                        \
    }                                                                     \
    virtual FDataReferenceCollection GetOutputs() const override          \
    {                                                                     \
        checkNoEntry();                                                   \
        return {};                                                        \
    }