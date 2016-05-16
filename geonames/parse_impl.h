#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "geonames.h"

namespace geonames {

template <typename T>
static std::u32string ToLower(const T& data) {
    std::u32string res(data.begin(), data.end());
    std::transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

bool ParseImpl(
    std::vector<ParseResult>& results,
    const std::string& query,
    const GeoData& data,
    const ParserSettings& settings
);

} // namespace geonames
