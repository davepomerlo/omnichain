#pragma once
#include "dsp/Effects.h"
#include <vector>
#include <cmath>

struct PresetSlot
{
    int type = oc::FxNone;
    float mix = NAN;              // % (NAN = effect's default mix)
    float level = 0.0f;           // dB
    std::vector<float> values;    // real values in parameter order; NAN or missing = default
    bool bypass = false;
};

struct PresetDef
{
    const char* name;
    std::vector<PresetSlot> slots;
    float inDb = 0.0f, outDb = 0.0f;
};

const std::vector<PresetDef>& getFactoryPresets();
