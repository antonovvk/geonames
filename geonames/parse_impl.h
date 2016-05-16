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

struct MatchedObject: public ParsedObject {
    std::vector<std::u32string> WideTokens_;
    bool ByName_ = false;
    bool Ambiguous_ = false;

    void Update(GeoObjectPtr obj, std::string token, std::u32string wideToken, bool byName);
};

struct MatchResult {
    MatchedObject Country_;
    MatchedObject Province_;
    MatchedObject City_;
    double Score_;

    void CalcScore(std::u32string query, std::string defaultCountryCode, bool areaToken);
};

bool ParseImpl(
    std::vector<ParseResult>& results,
    const std::string& query,
    const GeoData& data,
    const ParserSettings& settings
);

} // namespace geonames
