// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

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