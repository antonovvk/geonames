#include <algorithm>
#include <locale>
#include <codecvt>
#include <cassert>
#include <cerrno>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include "include/mms/features/hash/c++11.h"
#include "include/mms/vector.h"
#include "include/mms/string.h"
#include "include/mms/unordered_map.h"
#include "include/mms/writer.h"
#include "include/mms/ptr.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "geonames.h"

using namespace std;

namespace geonames {

string GeoTypeToString(GeoType type) {
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

GeoType GeoTypeFromString(const string& str) {
    for (uint32_t i = _TypesBegin; i <= _TypesEnd; ++i) {
        GeoType f = (GeoType)i;
        if (GeoTypeToString(f) == str) {
            return f;
        }
    }
    return _Undef;
}

template <typename T>
static u32string ToLower(const T& data) {
    u32string res(data.begin(), data.end());
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

template <typename P>
struct ObjectImpl {
    uint32_t Id_ = 0;
    GeoType Type_ = _Undef;
    double Latitude_ = 0;
    double Longitude_ = 0;
    size_t Population_ = 0;

    mms::vector<P, uint32_t> Name_;
    mms::vector<P, size_t> AltHashes_;
    mms::string<P> AsciiName_;
    mms::string<P> CountryCode_;
    mms::string<P> ProvinceCode_;

    ObjectImpl(const string& raw);

    uint64_t NameHash() const {
        return std::hash<u32string>()(ToLower(Name_));
    };

    void Merge(const ObjectImpl& obj) {
        assert(Id_ == obj.Id_);
        if (Population_ == 0) {
            Population_ = obj.Population_;
        }
    }

    template<class A> void traverseFields(A a) const {
        a(Id_)(Type_)(Latitude_)(Longitude_)(Population_)(Name_)(AltHashes_)(AsciiName_)(CountryCode_)(ProvinceCode_);
    }
};

typedef ObjectImpl<mms::Standalone> StandaloneObject;
typedef ObjectImpl<mms::Mmapped> MappedObject;

template <typename P>
ObjectImpl<P>::ObjectImpl(const string& raw)
{
    wstring_convert<codecvt_utf8<char32_t>, char32_t> utf32conv;
    stringstream columns(raw);
    string column;
    uint32_t idx = 0;

    while (getline(columns, column, '\t')) {
        switch (idx) {
            case 0: Id_ = stoi(column); break;
            case 1: {
                auto name = utf32conv.from_bytes(column);
                Name_.insert(Name_.end(), name.begin(), name.end());
                break;
            }
            case 2: AsciiName_ = column; break;
            case 3: {
                stringstream names(column);
                string name;
                while (getline(names, name, ',')) {
                    AltHashes_.push_back(hash<u32string>()(ToLower(utf32conv.from_bytes(name))));
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
}

template <typename P>
struct DataImpl {
    mms::unordered_map<P, uint32_t, ObjectImpl<P>> Objects_;
    mms::unordered_map<P, uint64_t, mms::vector<P, uint32_t>> IdsByNameHash_;
    mms::unordered_map<P, uint64_t, mms::vector<P, uint32_t>> IdsByAltHash_;
    mms::unordered_map<P, mms::string<P>, uint32_t> CountryByCode_;
    mms::unordered_map<P, mms::string<P>, uint32_t> ProvinceByCode_;

    void IdByHash(uint64_t hash, uint32_t id, bool alt) {
        auto& map = alt ? IdsByAltHash_ : IdsByNameHash_;
        map[hash].push_back(id);
    }

    template<class A> void traverseFields(A a) const {
        a(Objects_)(IdsByNameHash_)(IdsByAltHash_)(CountryByCode_)(ProvinceByCode_);
    }
};

typedef DataImpl<mms::Standalone> StandaloneData;
typedef DataImpl<mms::Mmapped> MappedData;

template <typename Impl>
class GeoObjectProxy: public GeoObject {
public:
    GeoObjectProxy(const Impl& impl)
        : Impl_(impl)
    {
    }

    virtual ~GeoObjectProxy()
    {
    }

    virtual uint32_t Id() const override {
        return Impl_.Id_;
    }

    virtual GeoType Type() const override {
        return Impl_.Type_;
    }

    virtual double Latitude() const override {
        return Impl_.Latitude_;
    }

    virtual double Longitude() const override {
        return Impl_.Longitude_;
    }

    virtual size_t Population() const override {
        return Impl_.Population_;
    }

    virtual u32string Name() const override {
        return u32string(Impl_.Name_.begin(), Impl_.Name_.end());
    }

    virtual string AsciiName() const override {
        return Impl_.AsciiName_;
    }

    virtual string CountryCode() const override {
        return Impl_.CountryCode_;
    }

    virtual string ProvinceCode() const override {
        return Impl_.ProvinceCode_;
    }

    virtual vector<size_t> AltHashes() const override {
        return vector<size_t>(Impl_.AltHashes_.begin(), Impl_.AltHashes_.end());
    }

private:
    const Impl& Impl_;
};

bool GeoObject::IsCountry() const {
    return Type() == _PolitIndep;
}

bool GeoObject::IsProvince() const {
    return Type() == _Adm1;// && Type_ <= _AdmDiv;
}

bool GeoObject::IsCity() const {
    return Type() >= _AdmEnd;
}

bool GeoObject::HasCountryCode() const {
    return !CountryCode().empty();
}

bool GeoObject::HasProvinceCode() const {
    return !ProvinceCode().empty();
}

class GeoNames::Impl {
public:
    Impl()
        : Data_(nullptr)
    {
    }

    bool Build(const string& mapFileName, const string& rawFileName, ostream& err) const {
        ifstream file(rawFileName);
        if (!file) {
            err << "Unable to open input file " << rawFileName << endl;
            return false;
        }
        StandaloneData data;

        size_t n = 0;
        string line;
        while (getline(file, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }

            StandaloneObject object(line);
            GeoObjectProxy<StandaloneObject> obj(object);
            if (obj.Type() == _Undef || obj.Type() & 1u) {
                continue;
            }

            auto it = data.Objects_.find(obj.Id());
            if (it != data.Objects_.end()) {
                it->second.Merge(object);
                continue;
            }

            data.Objects_.insert({ obj.Id(), object });
            data.IdByHash(object.NameHash(), obj.Id(), false);
            for (auto hash: obj.AltHashes()) {
                data.IdByHash(hash, obj.Id(), true);
            }
            if (obj.IsCountry()) {
                data.CountryByCode_.insert({ obj.CountryCode(), obj.Id() });
            }
            if (obj.IsProvince()) {
                data.ProvinceByCode_.insert({ obj.CountryCode() + obj.ProvinceCode(), obj.Id() });
            }
            ++n;
        }
        if (data.Objects_.empty()) {
            err << "No object was mapped" << endl;
            return false;
        }

        ofstream out(mapFileName);
        const size_t pos = mms::write(out, data);
        out.write((const char*)&pos, sizeof(size_t));
        out.close();
        return true;
    }

    bool Init(const string& mapFileName, ostream& err) {
        int fd = ::open(mapFileName.c_str(), O_RDONLY);
        if (fd >= 0) {
            struct stat st;
            if (fstat(fd, &st) == -1) {
                err << "Failed to stat map file: " << mapFileName << " error: " << strerror(errno) << endl;
                return false;
            }
            if (st.st_size <= (int)sizeof(size_t)) {
                err << "Invalid map file: " << mapFileName << " size: " << st.st_size << endl;
                return false;
            }
            const size_t size = st.st_size - sizeof(size_t);
            size_t pos;
            if (pread(fd, &pos, sizeof(size_t), size) != sizeof(size_t)) {
                err << "Failed to read map position from file: " << mapFileName << " error: " << strerror(errno) << endl;
                return false;
            }
            if (pos >= size) {
                err << "Invalid map position in file: " << mapFileName << endl;
                return false;
            }
            auto data = (char*) mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
            Data_ = reinterpret_cast<const MappedData*>(data + pos);
        } else {
            err << "Failed to open file: " << mapFileName << " error: " << strerror(errno) << endl;
            return false;
        }

        if (Data_) {
            size_t incompleteObjs = 0;
            for (auto& it: Data_->Objects_) {
                GeoObjectProxy<MappedObject> obj(it.second);
                auto country = Data_->CountryByCode_.find(obj.CountryCode());
                if (country == Data_->CountryByCode_.end()) {
                    //~ err << obj->Raw_ << endl;
                    ++incompleteObjs;
                    continue;
                }
                auto province = Data_->ProvinceByCode_.find(obj.CountryCode() + obj.ProvinceCode());
                if (province == Data_->ProvinceByCode_.end()) {
                    //~ err << obj->Raw_ << endl;
                    ++incompleteObjs;
                    continue;
                }
            }
            if (incompleteObjs) {
                err << "Have " << incompleteObjs << " objects without country or province code" << endl;
                //~ return false;
            }
        }
        return Data_ != nullptr;
    }

    bool Parse(vector<ParseResult>& results, const string& str, bool uniqueOnly) const {
        if (!Data_) {
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

        auto addObj = [&countries, &provinces, &cities, &utf8codec](GeoObjectPtr obj, const u32string& token, bool byName) {
            string name(utf8codec.to_bytes(token));
            if (obj->IsCountry()) {
                countries[obj->CountryCode()].Update(obj, name, byName);
            } else if (obj->IsProvince()) {
                provinces[obj->CountryCode() + obj->ProvinceCode()].Update(obj, name, byName);
            } else if (obj->IsCity()) {
                cities[obj->Id()].Update(obj, name, byName);
            }
        };

        for (auto& names: hypotheses) {
            for (auto& name: names) {
                auto it = Data_->IdsByNameHash_.find(std::hash<u32string>()(ToLower(name)));
                if (it == Data_->IdsByNameHash_.end()) {
                    continue;
                }
                for (auto& id: it->second) {
                    addObj(GetObj(id), name, true);
                }
            }
            for (auto& name: names) {
                auto it = Data_->IdsByAltHash_.find(std::hash<u32string>()(ToLower(name)));
                if (it == Data_->IdsByAltHash_.end()) {
                    continue;
                }
                for (auto& id: it->second) {
                    addObj(GetObj(id), name, false);
                }
            }
            if (names[0].size() == 2) {
                auto code = utf8codec.to_bytes(names[0]);
                if (code.size() == 2) {
                    code[0] = toupper(code[0]);
                    code[1] = toupper(code[1]);
                    auto it = Data_->CountryByCode_.find(code);
                    if (it != Data_->CountryByCode_.end()) {
                        addObj(GetObj(it->second), names[0], true);
                    }
                    it = Data_->ProvinceByCode_.find(string("US") + code);
                    if (it != Data_->ProvinceByCode_.end()) {
                        addObj(GetObj(it->second), names[0], true);
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
            if (!it.second || !(added.insert(it.second.Object_->Id())).second) {
                continue;
            }
            found.push_back(ParseResult());
            auto& res = found.back();
            auto obj = it.second.Object_.get();
            res.City_ = it.second;

            if (!obj->HasCountryCode()) {
                continue;
            }
            auto code = obj->CountryCode();
            auto c = countries.find(code);
            if (c != countries.end()) {
                res.Country_ = c->second;
                used.insert(code);
            }
            if (!obj->HasProvinceCode()) {
                continue;
            }
            code += obj->ProvinceCode();
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
            auto obj = it.second.Object_.get();
            res.Province_ = it.second;

            if (!obj->HasCountryCode()) {
                continue;
            }
            auto c = countries.find(obj->CountryCode());
            if (c != countries.end()) {
                res.Country_ = c->second;
                used.insert(obj->CountryCode());
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
            return score << matched.size();
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
                    auto countryCode = result.City_ ? result.City_.Object_->CountryCode() : result.Province_.Object_->CountryCode();
                    auto it = Data_->CountryByCode_.find(countryCode);
                    if (it != Data_->CountryByCode_.end()) {
                        result.Country_.Object_ = GetObj(it->second);
                    }
                }
                if (result.City_ && !result.Province_) {
                    auto it = Data_->ProvinceByCode_.find(result.City_.Object_->CountryCode() + result.City_.Object_->ProvinceCode());
                    if (it != Data_->ProvinceByCode_.end()) {
                        result.Province_.Object_ = GetObj(it->second);
                    }
                }
            }
        }
        // TODO: remove conflicts
        results.swap(tmp);
        return !results.empty();
    }

private:
    GeoObjectPtr GetObj(uint32_t id) const {
        auto it = Data_->Objects_.find(id);
        assert(it != Data_->Objects_.end());
        return GeoObjectPtr(new GeoObjectProxy<MappedObject>(it->second));
    }

    struct MatchedObject: public ParsedObject {
        void Update(GeoObjectPtr obj, string token, bool byName) {
            if (Ambiguous_) {
                return;
            } else if (!Object_) {
                Object_ = obj;
                Tokens_.push_back(token);
                ByName_ = byName;
            } else if (Object_->Id() != obj->Id()) {
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

private:
    const MappedData* Data_;
};

GeoNames::GeoNames()
    : Impl_(new Impl)
{
}

GeoNames::~GeoNames() = default;

bool GeoNames::Build(const string& mapFileName, const string& rawFileName, ostream& err) const {
    return Impl_->Build(mapFileName, rawFileName, err);
}

bool GeoNames::Init(const string& mapFileName, ostream& err) {
    return Impl_->Init(mapFileName, err);
}

bool GeoNames::Parse(vector<ParseResult>& results, const string& str, bool uniqueOnly) const {
    return Impl_->Parse(results, str, uniqueOnly);
}

} // namespace geonames
