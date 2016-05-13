#include <algorithm>
#include <locale>
#include <codecvt>
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include "geonames.h"

using namespace std;

namespace geonames {

string GeoTypeToString(_GeoType type) {
    switch (type) {
        case _Adm1:         return "ADM1";
        case _Adm2:         return "ADM2";
        case _Adm3:         return "ADM3";
        case _Adm4:         return "ADM4";
        case _Adm5:         return "ADM5";
        case _AdmDiv:       return "ADMD";
        case _AdmHist1:     return "ADM1H";
        case _AdmHist2:     return "ADM2H";
        case _AdmHist3:     return "ADM3H";
        case _AdmHist4:     return "ADM4H";
        case _AdmHistDiv:   return "ADMDH";

        case _PolitIndep:   return "PCLI";
        case _PolitSect:    return "PCLIX";
        case _PolitFree:    return "PCLF";
        case _PolitSemi:    return "PCLS";
        case _PolitDep:     return "PCLD";
        case _PolitHist:    return "PCLH";

        case _Popul:        return "PPL";
        case _PopulAdm1:    return "PPLA";
        case _PopulAdm2:    return "PPLA2";
        case _PopulAdm3:    return "PPLA3";
        case _PopulAdm4:    return "PPLA4";
        case _PopulCap:     return "PPLC";
        case _PopulGov:     return "PPLG";
        case _PopulPlace:   return "PPLS";
        case _PopulSect:    return "PPLX";
        case _PopulFarm:    return "PPLF";
        case _PopulLoc:     return "PPLL";
        case _PopulRelig:   return "PPLR";
        case _PopulAbandoned: return "PPLQ";
        case _PopulDestroyed: return "PPLW";
        case _PopulHist:    return "PPLH";
        case _PopulCapHist: return "PPLCH";
        default: break;
    }
    return "";
}

_GeoType GeoTypeFromString(const string& str) {
    for (uint32_t i = _TypesBegin; i <= _TypesEnd; ++i) {
        _GeoType f = (_GeoType)i;
        if (GeoTypeToString(f) == str) {
            return f;
        }
    }
    return _Undef;
}

static u32string toLower(const u32string& str) {
    u32string res(str);
    transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

/*
    http://download.geonames.org/export/dump/

    The main 'geoname' table has the following fields :
    ---------------------------------------------------
    geonameid         : integer id of record in geonames database
    name              : name of geographical point (utf8) varchar(200)
    asciiname         : name of geographical point in plain ascii characters, varchar(200)
    alternatenames    : alternatenames, comma separated, ascii names automatically transliterated, convenience attribute from alternatename table, varchar(10000)
    latitude          : latitude in decimal degrees (wgs84)
    longitude         : longitude in decimal degrees (wgs84)
    feature class     : see http://www.geonames.org/export/codes.html, char(1)
    feature code      : see http://www.geonames.org/export/codes.html, varchar(10)
    country code      : ISO-3166 2-letter country code, 2 characters
    cc2               : alternate country codes, comma separated, ISO-3166 2-letter country code, 200 characters
    admin1 code       : fipscode (subject to change to iso code), see exceptions below, see file admin1Codes.txt for display names of this code; varchar(20)
    admin2 code       : code for the second administrative division, a county in the US, see file admin2Codes.txt; varchar(80)
    admin3 code       : code for third level administrative division, varchar(20)
    admin4 code       : code for fourth level administrative division, varchar(20)
    population        : bigint (8 byte int)
    elevation         : in meters, integer
    dem               : digital elevation model, srtm3 or gtopo30, average elevation of 3''x3'' (ca 90mx90m) or 30''x30'' (ca 900mx900m) area in meters, integer. srtm processed by cgiar/ciat.
    timezone          : the timezone id (see file timeZone.txt) varchar(40)
    modification date : date of last modification in yyyy-MM-dd format
*/

GeoObject::GeoObject(const string& raw)
    : Raw_(raw)
{
    wstring_convert<codecvt_utf8<char32_t>, char32_t> utf32conv;
    stringstream columns(raw);
    string column;
    uint32_t idx = 0;

    while (getline(columns, column, '\t')) {
        switch (idx) {
            case 0: Id_ = stoi(column); break;
            case 1: Name_ = utf32conv.from_bytes(column); break;
            case 2: AsciiName_ = column; break;
            case 3: {
                stringstream names(column);
                string name;
                while (getline(names, name, ',')) {
                    AltHashes_.push_back(hash<u32string>()(toLower(utf32conv.from_bytes(name))));
                }
                break;
            }
            case 4: Latitude_ = stod(column); break;
            case 5: Longitude_ = stod(column); break;
            case 7: Type_ = GeoTypeFromString(column); break;
            case 8: CountryCode_ = column; break;
            case 10: ProvinceCode_ = column; break;
            case 14: Population_ = atol(column.c_str()); break;
            default: break; // TODO
        }
        ++idx;
    }
};

void GeoObject::Merge(const GeoObject& obj) {
    assert(Id_ == obj.Id_);
    if (Population_ == 0) {
        Population_ = obj.Population_;
    }
}

bool GeoObject::IsCountry() const {
    return Type_ == _PolitIndep;
}

bool GeoObject::IsProvince() const {
    return Type_ == _Adm1;// && Type_ <= _AdmDiv;
}

bool GeoObject::IsCity() const {
    return Type_ >= _AdmEnd;
}

bool GeoObject::HasCountryCode() const {
    return !CountryCode_.empty();
}

bool GeoObject::HasProvinceCode() const {
    return !ProvinceCode_.empty();
}

class GeoNames::Impl {
    typedef unique_ptr<GeoObject> GeoObjectPtr;

public:
    Impl()
    {
    }

    bool LoadData(const string& fileName, bool saveRaw) {
        ifstream file(fileName);
        if (!file) {
            return false;
        }
        string line;
        bool firstLine = true;
        while (getline(file, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            if (firstLine) {
                firstLine = false;
                continue;
            }

            Objects_.emplace_back(new GeoObject(line));
            auto& obj = Objects_.back();
            if (!saveRaw) {
                string().swap(obj->Raw_);
            }
            if (obj->Type_ == _Undef || obj->Type_ & 1u) {
                Objects_.pop_back();
                continue;
            }
            auto it = ObjById_.find(obj->Id_);
            if (it != ObjById_.end()) {
                it->second->Merge(*obj);
                Objects_.pop_back();
                continue;
            }

            ObjById_.insert({ obj->Id_, obj.get() });
            ObjsByName_.insert(obj.get());
            for (size_t i = 0; i < obj->AltHashes_.size(); ++i) {
                ObjsByAltHash_.insert({ obj.get(), i });
            }
            if (obj->IsCountry()) {
                CountryByCode_.insert({ obj->CountryCode_, obj.get() });
            }
            if (obj->IsProvince()) {
                ProvinceByCode_.insert({ obj->CountryCode_ + obj->ProvinceCode_, obj.get() });
            }
        }
        return true;
    }

    bool Init(ostream& err) {
        if (!Initialized_) {
            size_t incompleteObjs = 0;
            for (auto& obj: Objects_) {
                auto country = CountryByCode_.find(obj->CountryCode_);
                if (country == CountryByCode_.end()) {
                    //~ err << obj->Raw_ << endl;
                    ++incompleteObjs;
                    continue;
                }
                auto province = ProvinceByCode_.find(obj->CountryCode_ + obj->ProvinceCode_);
                if (province == ProvinceByCode_.end()) {
                    //~ err << obj->Raw_ << endl;
                    ++incompleteObjs;
                    continue;
                }
            }
            if (incompleteObjs) {
                err << "Have " << incompleteObjs << " objects without country or province code" << endl;
                //~ return false;
            }
            Initialized_ = true;
        }
        return Initialized_;
    }

    bool Parse(vector<ParseResult>& results, const string& str, bool uniqueOnly) const {
        if (!Initialized_) {
            return false;
        }
        wstring_convert<codecvt_utf8<char32_t>, char32_t> utf8codec;
        const auto wstr = utf8codec.from_bytes(str);

        vector<u32string> tokens;
        vector<u32string> delims;
        u32string delim;

        size_t pos = 0;
        while (pos < wstr.size()) {
            size_t next = 0;
            while (pos < wstr.size() && (next = wstr.find_first_of(U"\t .;,/&()", pos)) == pos) {
                delim.append(1, wstr[pos]);
                ++pos;
            }
            if (pos == wstr.size()) {
                break;
            }
            if (!tokens.empty()) {
                delims.push_back(delim);
            }
            delim.clear();
            tokens.push_back(wstr.substr(pos, next - pos));
            pos = next;
        }
        if (!tokens.empty()) {
            delims.push_back(delim);
        }
        vector<vector<u32string>> hypotheses(1);
        hypotheses[0].push_back(wstr);
        for (uint32_t idx = 0; idx < tokens.size(); ++idx) {
            hypotheses.push_back(vector<u32string>());
            auto& names = hypotheses.back();
            u32string combined;
            for (uint32_t extra = idx; extra < min<size_t>(idx + 3, tokens.size()); ++extra) {
                combined += tokens[extra];
                names.push_back(combined);
                combined += delims[extra];
            }
            if (idx + 1 < tokens.size() && delims[idx].find_first_not_of(U"\t ") == string::npos) {
                combined = tokens[idx] + tokens[idx + 1];
                names.push_back(combined);
            }
        }

        unordered_map<string, MatchedObject> countries;
        unordered_map<string, MatchedObject> provinces;
        unordered_map<uint32_t, MatchedObject> cities;

        auto addObj = [&countries, &provinces, &cities, &utf8codec](const GeoObject* obj, const u32string& token, bool byName) {
            string name(utf8codec.to_bytes(token));
            if (obj->IsCountry()) {
                countries[obj->CountryCode_].Update(obj, name, byName);
            } else if (obj->IsProvince()) {
                provinces[obj->CountryCode_ + obj->ProvinceCode_].Update(obj, name, byName);
            } else if (obj->IsCity()) {
                cities[obj->Id_].Update(obj, name, byName);
            }
        };

        for (auto& names: hypotheses) {
            GeoObject obj;
            for (auto& name: names) {
                obj.Name_ = name;
                auto range = ObjsByName_.equal_range(&obj);
                for (auto it = range.first; it != range.second; ++it) {
                    addObj(*it, name, true);
                }
            }
            for (auto& name: names) {
                vector<size_t>(1, hash<u32string>()(toLower(name))).swap(obj.AltHashes_);
                auto range = ObjsByAltHash_.equal_range({ &obj, 0 });
                for (auto it = range.first; it != range.second; ++it) {
                    addObj(it->first, name, false);
                }
            }
            if (names[0].size() == 2) {
                auto code = utf8codec.to_bytes(names[0]);
                if (code.size() == 2) {
                    code[0] = toupper(code[0]);
                    code[1] = toupper(code[1]);
                    auto it = CountryByCode_.find(code);
                    if (it != CountryByCode_.end()) {
                        addObj(it->second, names[0], true);
                    }
                    it = ProvinceByCode_.find(string("US") + code);
                    if (it != ProvinceByCode_.end()) {
                        addObj(it->second, names[0], true);
                    }
                }
            }
            if (names[0] == wstr && (!countries.empty() || !provinces.empty() || !cities.empty())) {
                break;
            }
        }

        vector<ParseResult> found;
        unordered_set<string> used;
        unordered_set<uint32_t> added;
        for (auto it: cities) {
            if (!it.second || !(added.insert(it.second.Object_->Id_)).second) {
                continue;
            }
            found.push_back(ParseResult());
            auto& res = found.back();
            auto obj = it.second.Object_;
            res.City_ = it.second;

            if (!obj->HasCountryCode()) {
                continue;
            }
            auto code = obj->CountryCode_;
            auto c = countries.find(code);
            if (c != countries.end()) {
                res.Country_ = c->second;
                used.insert(code);
            }
            if (!obj->HasProvinceCode()) {
                continue;
            }
            code += obj->ProvinceCode_;
            auto p = provinces.find(code);
            if (p != provinces.end()) {
                res.Province_ = p->second;
                used.insert(code);
            }
        }
        for (auto& it: provinces) {
            if (!it.second || used.find(it.first) != used.end()) {
                continue;
            }
            found.push_back(ParseResult());
            auto& res = found.back();
            auto obj = it.second.Object_;
            res.Province_ = it.second;

            if (!obj->HasCountryCode()) {
                continue;
            }
            auto c = countries.find(obj->CountryCode_);
            if (c != countries.end()) {
                res.Country_ = c->second;
                used.insert(obj->CountryCode_);
            }
        }
        for (auto& it: countries) {
            if (!it.second || used.find(it.first) != used.end()) {
                continue;
            }
            found.push_back(ParseResult());
            auto& res = found.back();
            res.Country_ = it.second;
        }

        auto calcScore = [](const ParseResult& res) {
            size_t score = 0;
            size_t scores[] = { 3, 2, 1 };
            ParsedObject objs[] = { res.Country_, res.Province_, res.City_ };
            unordered_set<string> matched;
            for (uint32_t idx = 0; idx < 3; ++idx) {
                if (objs[idx].Object_) {
                    score += scores[idx];
                    if (objs[idx].ByName_) {
                        ++score;
                    }
                    for (auto name: objs[idx].Tokens_) {
                        matched.insert(name);
                    }
                }
            }
            score = score << matched.size();
            return score;
        };

        size_t maxScore = 0;
        size_t maxScoreCount = 0;
        for (auto& res: found) {
            auto score = calcScore(res);
            if (maxScore < score) {
                maxScore = score;
                maxScoreCount = 1;
            } else if (maxScore == score) {
                ++maxScoreCount;
            }
        }
        if (uniqueOnly && maxScoreCount > 1) {
            return false;
        }

        vector<ParseResult> tmp;
        for (auto& res: found) {
            // TODO: do not recalculate score
            if (calcScore(res) == maxScore) {
                tmp.push_back(res);
                auto& result = tmp.back();
                result.Score_ = maxScore;
                if (!result.Country_) {
                    assert(result.City_ || result.Province_);
                    auto countryCode = result.City_ ? result.City_.Object_->CountryCode_ : result.Province_.Object_->CountryCode_;
                    auto it = CountryByCode_.find(countryCode);
                    if (it != CountryByCode_.end()) {
                        result.Country_.Object_ = it->second;
                    }
                }
                if (result.City_ && !result.Province_) {
                    auto it = ProvinceByCode_.find(result.City_.Object_->CountryCode_ + result.City_.Object_->ProvinceCode_);
                    if (it != ProvinceByCode_.end()) {
                        result.Province_.Object_ = it->second;
                    }
                }
            }
        }
        // TODO: remove conflicts
        results.swap(tmp);
        return !results.empty();
    }

private:
    struct MatchedObject: public ParsedObject {
        void Update(const GeoObject* obj, string token, bool byName) {
            if (Ambiguous_) {
                return;
            } else if (!Object_) {
                Object_ = obj;
                Tokens_.push_back(token);
                ByName_ = byName;
            } else if (Object_->Id_ != obj->Id_) {
                Object_ = nullptr;
                Tokens_.clear();
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
                }
                ByName_ |= byName;
            }
        }
    };

    struct GeoObjNameHash {
        size_t operator() (const GeoObject* p) const {
            return std::hash<u32string>()(toLower(p->Name_));
        }
    };
    struct GeoObjNameEqual {
        bool operator() (const GeoObject* a, const GeoObject* b) const {
            return toLower(a->Name_) == toLower(b->Name_);
        }
    };

    struct GeoObjAltNameHash {
        size_t operator() (pair<const GeoObject*, size_t> p) const {
            return p.first->AltHashes_[p.second];
        }
    };
    struct GeoObjAltNameEqual {
        bool operator() (pair<const GeoObject*, size_t> a, pair<const GeoObject*, size_t> b) const {
            return a.first->AltHashes_[a.second] == b.first->AltHashes_[b.second];
        }
    };

private:
    unordered_map<uint32_t, GeoObject*> ObjById_;
    unordered_map<string, const GeoObject*> CountryByCode_;
    unordered_map<string, const GeoObject*> ProvinceByCode_;
    unordered_multiset<const GeoObject*, GeoObjNameHash, GeoObjNameEqual> ObjsByName_;
    unordered_multiset<pair<const GeoObject*, size_t>, GeoObjAltNameHash, GeoObjAltNameEqual> ObjsByAltHash_;
    vector<GeoObjectPtr> Objects_;
    bool Initialized_ = false;
};

GeoNames::GeoNames()
    : Impl_(new Impl)
{
}

GeoNames::~GeoNames() = default;

bool GeoNames::LoadData(const string& fileName, bool saveRaw) {
    return Impl_->LoadData(fileName, saveRaw);
}

bool GeoNames::Init(ostream& err) {
    return Impl_->Init(err);
}

bool GeoNames::Parse(vector<ParseResult>& results, const string& str, bool uniqueOnly) const {
    return Impl_->Parse(results, str, uniqueOnly);
}

} // namespace geonames
