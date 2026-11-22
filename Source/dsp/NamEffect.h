// OmniChain - Neural Amp Modeler (.nam) support, built on the official NeuralAmpModelerCore (MIT)
#pragma once
#include "Effects.h"
#include <string>

namespace oc
{
struct ResourceLoadResult
{
    std::shared_ptr<const Resource> resource;
    std::string error;
};

// Parse and validate a .nam file (control thread). Path is UTF-8.
// light: captures that carry several sizes in one file (NAM A2 Full + Lite) run their smallest size.
ResourceLoadResult loadNamFile (const std::string& utf8Path, bool light = true);
} // namespace oc
