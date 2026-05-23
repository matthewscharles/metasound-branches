// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

// Centralized node style keys for MetaSoundBranches.
#define METASOUND_BRANCHES_STYLE_SET_NAME TEXT("MetaSoundStyle")
#define METASOUND_BRANCHES_ICON_KEY TEXT("MetasoundEditor.Graph.Node.Custom.Branches.Icon")
#define METASOUND_BRANCHES_COLOR_KEY_AUDIO TEXT("MetasoundEditor.Graph.Node.Custom.Branches.Color.Audio")
#define METASOUND_BRANCHES_COLOR_KEY_FLOAT TEXT("MetasoundEditor.Graph.Node.Custom.Branches.Color.Float")
#define METASOUND_BRANCHES_COLOR_KEY_TRIGGER TEXT("MetasoundEditor.Graph.Node.Custom.Branches.Color.Trigger")
#define METASOUND_BRANCHES_COLOR_KEY_INT TEXT("MetasoundEditor.Graph.Node.Custom.Branches.Color.Int")

// Applies a consistent style set, icon, and node title color by class variant.
#define METASOUND_BRANCHES_APPLY_NODE_STYLE(MetadataVar)                                     \
    do                                                                                        \
    {                                                                                         \
        (MetadataVar).DisplayStyle.StyleSet = FName(METASOUND_BRANCHES_STYLE_SET_NAME);      \
        (MetadataVar).DisplayStyle.Icon = FName(METASOUND_BRANCHES_ICON_KEY);                \
                                                                                              \
        const FName MetaSoundBranchesNodeVariant = (MetadataVar).ClassName.GetVariant();     \
        if (MetaSoundBranchesNodeVariant == FName(TEXT("Audio")))                             \
        {                                                                                     \
            (MetadataVar).DisplayStyle.Color = FName(METASOUND_BRANCHES_COLOR_KEY_AUDIO);    \
        }                                                                                     \
        else if (MetaSoundBranchesNodeVariant == FName(TEXT("Float")))                        \
        {                                                                                     \
            (MetadataVar).DisplayStyle.Color = FName(METASOUND_BRANCHES_COLOR_KEY_FLOAT);    \
        }                                                                                     \
        else if (MetaSoundBranchesNodeVariant == FName(TEXT("Trigger")))                      \
        {                                                                                     \
            (MetadataVar).DisplayStyle.Color = FName(METASOUND_BRANCHES_COLOR_KEY_TRIGGER);  \
        }                                                                                     \
        else if (MetaSoundBranchesNodeVariant == FName(TEXT("Int32")) ||                      \
                 MetaSoundBranchesNodeVariant == FName(TEXT("Int")))                          \
        {                                                                                     \
            (MetadataVar).DisplayStyle.Color = FName(METASOUND_BRANCHES_COLOR_KEY_INT);      \
        }                                                                                     \
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