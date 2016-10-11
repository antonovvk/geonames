#include <locale>
#include <codecvt>
#include <cassert>
#include <unordered_map>
#include <unordered_set>

#include "parse_impl.h"
#include "geonames.h"

using namespace std;

namespace geonames {

struct MatchedObject: public ParsedObject {
    std::vector<std::u32string> WideTokens_;
    bool ByName_ = false;
    bool Ambiguous_ = false;

    void Update(GeoObjectPtr obj, std::string token, std::u32string wideToken, bool byName);
};

void MatchedObject::Update(GeoObjectPtr obj, string token, u32string wideToken, bool byName) {
    if (Ambiguous_) {
        return;
    } else if (!Object_) {
        Object_ = obj;
        Tokens_.push_back(token);
        WideTokens_.push_back(wideToken);
        ByName_ = byName;
    } else if (Object_->Id() != obj->Id()) {
        Object_ = nullptr;
        Tokens_.clear();
        WideTokens_.clear();
        ByName_ = false;
        Ambiguous_ = true;
    } else {
        bool found = false;
        for (auto& t: Tokens_) {
            if (t.find(token) != string::npos) {
                found = true;
                break;
            }
            if (token.find(t) != string::npos) {
                t = token;
                break;
            }
        }
        if (!found) {
            Tokens_.push_back(token);
            WideTokens_.push_back(wideToken);
        }
        ByName_ |= byName;
    }
}

struct MatchResult {
    MatchedObject Country_;
    MatchedObject Province_;
    MatchedObject City_;
    double Score_;

    void CalcScore(std::u32string query, std::string defaultCountryCode, bool areaToken);
};

void MatchResult::CalcScore(u32string query, string defaultCountryCode, bool areaToken) {
    double score = 0;
    double tokenScore = 1;
    double scores[] = { 3, 2, 1 };
    bool defaultCountryMet = false;
    const MatchedObject* objs[] = { &Country_, &Province_, &City_ };

    for (uint32_t idx = 0; idx < 3; ++idx) {
        if (objs[idx]->Object_) {
            score += scores[idx];
            if (objs[idx]->ByName_) {
                ++score;
            }
            if (!defaultCountryMet && defaultCountryCode == objs[idx]->Object_->CountryCode()) {
                score += 3;
                defaultCountryMet = true;
            }
            for (auto token: objs[idx]->WideTokens_) {
                tokenScore *= 1.0 * token.size() / query.size();
            }
        }
    }
    // TODO: fix this hack
    if (areaToken && City_ && City_.Object_->CountryCode() == "US" && City_.Object_->Type() == _PopulAdm1) {
        score += 3;
    }
    Score_ = score * (1 + tokenScore);
}

class Parser {
    struct Hypothesis {
        vector<u32string> Names_;
    };

public:
    Parser(const GeoData& data, const string& query, const ParserSettings& settings)
        : Settings_(settings)
        , Data_(data)
        , Query_(Utf8Codec_.from_bytes(query))
        , DelimSet_(Utf8Codec_.from_bytes(Settings_.Delimiters_))
        , AreaToken_(false)
    {
        u32string delim;
        size_t pos = 0;
        while (pos < Query_.size()) {
            size_t next = 0;
            while (pos < Query_.size() && (next = Query_.find_first_of(DelimSet_, pos)) == pos) {
                delim.append(1, Query_[pos]);
                ++pos;
            }
            if (pos == Query_.size()) {
                break;
            }
            if (!Tokens_.empty()) {
                Delims_.push_back(delim);
            }
            delim.clear();
            Tokens_.push_back(Query_.substr(pos, next - pos));
            pos = next;

            // Hack, do something with this
            if (ToLower(Tokens_.back()) == U"area") {
                AreaToken_ = true;
            }
        }
        if (!Tokens_.empty()) {
            Delims_.push_back(delim);
        }
    }

    bool Parse(vector<ParseResult>& results) {
        MakeHypotheses();

        vector<MatchResult> matched;
        RunMatching(matched);

        vector<ParseResult> tmp;
        RunScoring(tmp, matched);

        if (Settings_.UniqueOnly_ && tmp.size() > 1) {
            return false;
        }
        // TODO: remove conflicts
        results.swap(tmp);
        return !results.empty();
    }

private:
    void MakeHypotheses() {
        Countries_.clear();
        Provinces_.clear();
        Cities_.clear();

        vector<Hypothesis> hypotheses;

        Hypothesis hypo;
        hypo.Names_.push_back(Query_);
        hypotheses.push_back(hypo);

        for (uint32_t idx = 0; idx < Tokens_.size(); ++idx) {
            hypotheses.push_back(Hypothesis());
            auto& names = hypotheses.back().Names_;
            u32string combined;
            bool untrivialDelim = false;
            for (uint32_t extra = idx; extra < min<size_t>(idx + 3, Tokens_.size()); ++extra) {
                combined += Tokens_[extra];
                names.push_back(combined);
                combined += Delims_[extra];
                if (Delims_[extra].find_first_not_of(U" ") != string::npos) {
                    untrivialDelim = true;
                }
            }
            if (untrivialDelim) {
                combined.clear();
                for (uint32_t extra = idx; extra < min<size_t>(idx + 3, Tokens_.size()); ++extra) {
                    combined += Tokens_[extra];
                    names.push_back(combined);
                    combined += U" ";
                }
            }
            if (idx + 1 < Tokens_.size() && Delims_[idx].find_first_not_of(U"\t ") == string::npos) {
                combined = Tokens_[idx] + Tokens_[idx + 1];
                names.push_back(combined);
            }
        }

        for (auto& hypo: hypotheses) {
            assert(!hypo.Names_.empty());

            for (auto& name: hypo.Names_) {
                auto p = Data_.IdsByNameHash(std::hash<u32string>()(ToLower(name)));
                for (auto it = p.first; it != p.second; ++it) {
                    AddObject(*it, name, true);
                }
            }
            for (auto& name: hypo.Names_) {
                auto p = Data_.IdsByAltHash(std::hash<u32string>()(ToLower(name)));
                for (auto it = p.first; it != p.second; ++it) {
                    AddObject(*it, name, false);
                }
            }
            if (hypo.Names_[0].size() == 2) {
                auto code = Utf8Codec_.to_bytes(hypo.Names_[0]);
                if (code.size() == 2) {
                    code[0] = toupper(code[0]);
                    code[1] = toupper(code[1]);
                    auto it = Data_.CountryByCode(code);
                    if (it) {
                        AddObject(*it, hypo.Names_[0], true);
                    }
                    it = Data_.ProvinceByCode(string("US") + code);
                    if (it) {
                        AddObject(*it, hypo.Names_[0], true);
                    }
                }
            }
            if (hypo.Names_[0] == Query_ && (!Countries_.empty() || !Provinces_.empty() || !Cities_.empty())) {
                break;
            }
        }
    }

    void AddObject(uint32_t id, const u32string& token, bool byName) {
        auto obj = Data_.GetObject(id);
        assert(obj);

        string name(Utf8Codec_.to_bytes(token));
        if (obj->IsCountry()) {
            Countries_[obj->CountryCode()].Update(obj, name, token, byName);
        } else if (obj->IsProvince()) {
            Provinces_[obj->CountryCode() + obj->ProvinceCode()].Update(obj, name, token, byName);
        } else if (obj->IsCity()) {
            Cities_[obj->Id()].Update(obj, name, token, byName);
        }
    };

    void RunMatching(vector<MatchResult>& matched) {
        unordered_set<string> used;
        unordered_set<uint32_t> added;
        for (auto it: Cities_) {
            if (!it.second || !(added.insert(it.second.Object_->Id())).second) {
                continue;
            }
            auto obj = it.second.Object_.get();
            MatchResult res;
            res.City_ = it.second;

            SetCountryOrProvince(res, used, obj->CountryCode(), true);
            SetCountryOrProvince(res, used, obj->CountryCode() + obj->ProvinceCode(), false);
            matched.push_back(res);
        }
        for (auto& it: Provinces_) {
            if (!it.second || used.find(it.first) != used.end()) {
                continue;
            }
            auto obj = it.second.Object_.get();
            MatchResult res;
            res.Province_ = it.second;

            SetCountryOrProvince(res, used, obj->CountryCode(), true);
            matched.push_back(res);
        }
        for (auto& it: Countries_) {
            if (!it.second || used.find(it.first) != used.end()) {
                continue;
            }
            MatchResult res;
            res.Country_ = it.second;
            matched.push_back(res);
        }
    }

    void SetCountryOrProvince(MatchResult& res, unordered_set<string>& used, string code, bool country) const {
        if (!code.empty()) {
            auto& map = country ? Countries_ : Provinces_;
            auto it = map.find(code);
            if (it != map.end()) {
                auto& obj = country ? res.Country_ : res.Province_;
                obj = it->second;
                used.insert(code);
            }
        }
    }

    void RunScoring(vector<ParseResult>& results, vector<MatchResult>& matched) const {
        string defaultCountryCode;
        if (!matched.empty() && !Settings_.DefaultCountry_.empty()) {
            vector<ParseResult> tmp;
            ParserSettings tmpSettings;
            tmpSettings.UniqueOnly_ = true;
            if (ParseImpl(tmp, Settings_.DefaultCountry_, Data_, tmpSettings)) {
                if (tmp[0].Country_) {
                    defaultCountryCode = tmp[0].Country_.Object_->CountryCode();
                }
            }
        }

        double maxScore = 0;
        size_t maxScoreCount = 0;
        unordered_map<string, GeoObjectPtr> maxScoreCities;
        unordered_set<uint32_t> merged;

        for (auto& res: matched) {
            res.CalcScore(Query_, defaultCountryCode, AreaToken_);
            if (maxScore < res.Score_) {
                maxScore = res.Score_;
                maxScoreCount = 1;
                maxScoreCities.clear();
                AddCity(maxScoreCities, merged, res);
            } else if (maxScore == res.Score_) {
                ++maxScoreCount;
                AddCity(maxScoreCities, merged, res);
            }
        }

        for (auto& res: matched) {
            if (res.Score_ == maxScore) {
                if (res.City_ && merged.find(res.City_.Object_->Id()) != merged.end()) {
                    continue;
                }
                ParseResult result;
                result.Country_ = res.Country_;
                result.Province_ = res.Province_;
                result.City_ = res.City_;
                result.Score_ = res.Score_;
                if (!result.Country_) {
                    assert(result.City_ || result.Province_);
                    auto countryCode = result.City_ ? result.City_.Object_->CountryCode() : result.Province_.Object_->CountryCode();
                    auto it = Data_.CountryByCode(countryCode);
                    if (it) {
                        result.Country_.Object_ = Data_.GetObject(*it);
                    }
                }
                if (result.City_ && !result.Province_) {
                    auto it = Data_.ProvinceByCode(result.City_.Object_->CountryCode() + result.City_.Object_->ProvinceCode());
                    if (it) {
                        result.Province_.Object_ = Data_.GetObject(*it);
                    }
                }
                results.push_back(result);
            }
        }
    }

    void AddCity(
        unordered_map<string, GeoObjectPtr>& maxScoreCities,
        unordered_set<uint32_t>& merged,
        const MatchResult& res
    ) const {
        if (res.City_) {
            auto obj = res.City_.Object_;
            auto key = obj->CountryCode() + obj->ProvinceCode() + obj->AsciiName();
            auto it = maxScoreCities.insert({ key, obj });
            if (!it.second && (it.first->second->HaversineDistance(*obj) < Settings_.MergeNear_)) {
                merged.insert(obj->Id());
            }
        }
    }

private:
    wstring_convert<codecvt_utf8<char32_t>, char32_t> Utf8Codec_;
    const ParserSettings& Settings_;
    const GeoData& Data_;
    const u32string Query_;
    const u32string DelimSet_;
    vector<u32string> Tokens_;
    vector<u32string> Delims_;
    bool AreaToken_;
    unordered_map<string, MatchedObject> Countries_;
    unordered_map<string, MatchedObject> Provinces_;
    unordered_map<uint32_t, MatchedObject> Cities_;
};

bool ParseImpl(
    vector<ParseResult>& results,
    const std::string& query,
    const GeoData& data,
    const ParserSettings& settings
) {
    Parser parser(data, query, settings);
    return parser.Parse(results);
}

} // namespace geonames
