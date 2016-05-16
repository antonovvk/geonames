#include <locale>
#include <codecvt>
#include <cassert>
#include <cerrno>
#include <iostream>
#include <fstream>
#include <sstream>

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
#include "parse_impl.h"

using namespace std;

namespace geonames {

static const double PI = 3.14159265358979323846;
static const double EARTH_RADIUS_KM = 6371.0;

double Deg2Rad(double deg) {
    return (deg * PI / 180);
}

double Rad2Deg(double rad) {
    return (rad * 180 / PI);
}

/**
 * Returns the distance between two points on the Earth.
 * Direct translation from http://en.wikipedia.org/wiki/Haversine_formula
 * @param lat1d Latitude of the first point in degrees
 * @param lon1d Longitude of the first point in degrees
 * @param lat2d Latitude of the second point in degrees
 * @param lon2d Longitude of the second point in degrees
 * @return The distance between the two points in kilometers
 */
double HaversineDistance(double lat1d, double lon1d, double lat2d, double lon2d) {
    double lat1r, lon1r, lat2r, lon2r, u, v;
    lat1r = Deg2Rad(lat1d);
    lon1r = Deg2Rad(lon1d);
    lat2r = Deg2Rad(lat2d);
    lon2r = Deg2Rad(lon2d);
    u = sin((lat2r - lat1r)/2);
    v = sin((lon2r - lon1r)/2);
    return 2.0 * EARTH_RADIUS_KM * asin(sqrt(u * u + cos(lat1r) * cos(lat2r) * v * v));
}

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

        case _AreaRegion:       return "RGN";
        case _AreaRegionEcon:   return "RGNE";
        case _AreaRegionHist:   return "RGNH";
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

template <typename Impl>
GeoObjectPtr MakeGeoObject(const Impl& impl) {
    return GeoObjectPtr(new GeoObjectProxy<Impl>(impl));
}

template <typename Impl>
class GeoDataProxy: public GeoData {
public:
    GeoDataProxy(const Impl& impl)
        : Impl_(impl)
    {
    }

    virtual ~GeoDataProxy()
    {
    }

    virtual GeoObjectPtr GetObject(uint32_t id) const override {
        auto it = Impl_.Objects_.find(id);
        assert(it != Impl_.Objects_.end());
        return MakeGeoObject(it->second);
    }

    virtual pair<const uint32_t*, const uint32_t*> IdsByNameHash(uint64_t hash) const override {
        auto it = Impl_.IdsByNameHash_.find(hash);
        if (it != Impl_.IdsByNameHash_.end()) {
            return { it->second.begin(), it->second.end() };
        }
        return { nullptr, nullptr };
    }

    virtual pair<const uint32_t*, const uint32_t*> IdsByAltHash(uint64_t hash) const override {
        auto it = Impl_.IdsByAltHash_.find(hash);
        if (it != Impl_.IdsByAltHash_.end()) {
            return { it->second.begin(), it->second.end() };
        }
        return { nullptr, nullptr };
    }

    virtual const uint32_t* CountryByCode(const std::string& code) const override {
        auto it = Impl_.CountryByCode_.find(code);
        if (it != Impl_.CountryByCode_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    virtual const uint32_t* ProvinceByCode(const std::string& code) const override {
        auto it = Impl_.ProvinceByCode_.find(code);
        if (it != Impl_.ProvinceByCode_.end()) {
            return &it->second;
        }
        return nullptr;
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

double GeoObject::HaversineDistance(const GeoObject& obj) const {
    return geonames::HaversineDistance(Latitude(), Longitude(), obj.Latitude(), obj.Longitude());
}

class GeoNames::Impl {
public:
    Impl()
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
            Data_.reset(new GeoDataProxy<MappedData>(*reinterpret_cast<const MappedData*>(data + pos)));
        } else {
            err << "Failed to open file: " << mapFileName << " error: " << strerror(errno) << endl;
            return false;
        }
        return Data_ != nullptr;
    }

    bool Parse(vector<ParseResult>& results, const string& str, const ParserSettings& settings) const {
        if (!Data_) {
            return false;
        }
        return ParseImpl(results, str, *Data_, settings);
    }

private:
    unique_ptr<GeoData> Data_;
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

bool GeoNames::Parse(vector<ParseResult>& results, const string& str, const ParserSettings& settings) const {
    return Impl_->Parse(results, str, settings);
}

} // namespace geonames
