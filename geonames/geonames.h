#pragma once

#include <string>
#include <memory>
#include <vector>
#include <iostream>

namespace geonames {

enum _GeoType {
    _Undef = 0,

    _PolitIndep = 2,
    _PolitSect  = 4,
    _PolitFree  = 6,
    _PolitSemi  = 8,
    _PolitDep   = 10,
    _PolitHist  = 11,   // Odd values used for extra features set
    _PolitEnd   = 12,

    _Adm1       = 12,
    _Adm2       = 14,
    _Adm3       = 16,
    _Adm4       = 18,
    _Adm5       = 20,
    _AdmDiv     = 22,
    _AdmHist1   = 23,
    _AdmHist2   = 25,
    _AdmHist3   = 27,
    _AdmHist4   = 29,
    _AdmHistDiv = 31,
    _AdmEnd     = 32,

    _PopulCap   = 32,
    _PopulGov   = 34,
    _PopulAdm1  = 36,
    _PopulAdm2  = 38,
    _PopulAdm3  = 40,
    _PopulAdm4  = 42,
    _PopulPlace = 44,
    _Popul      = 46,
    _PopulSect  = 47,
    _PopulFarm  = 49,
    _PopulLoc   = 51,
    _PopulRelig = 53,
    _PopulAbandoned = 55,
    _PopulDestroyed = 57,
    _PopulHist      = 59,
    _PopulCapHist   = 61,
    _PopulEnd       = 62,

    _TypesBegin = _PolitIndep,
    _TypesMain  = _PopulSect,
    _TypesEnd   = _PopulCapHist
};

_GeoType GeoTypeFromString(const std::string& str);
std::string GeoTypeToString(_GeoType type);

struct GeoObject {
    uint32_t Id_ = 0;
    _GeoType Type_ = _Undef;
    double Latitude_ = 0;
    double Longitude_ = 0;
    std::u32string Name_;
    std::string AsciiName_;
    std::vector<size_t> AltHashes_;
    std::string CountryCode_;
    std::string ProvinceCode_;
    size_t Population_ = 0;
    std::string Raw_;

    GeoObject(const std::string& raw = "");

    void Merge(const GeoObject& obj);
    bool IsCountry() const;
    bool IsProvince() const;
    bool IsCity() const;

    bool HasCountryCode() const;
    bool HasProvinceCode() const;
};

struct ParsedObject {
    const GeoObject* Object_ = nullptr;
    std::vector<std::string> Tokens_;
    bool ByName_ = false;
    bool Ambiguous_ = false;

    operator bool() const {
        return Object_ != nullptr;
    }
};

struct ParseResult {
    ParsedObject Country_;
    ParsedObject Province_;
    ParsedObject City_;
    size_t Score_ = 0;
};

class GeoNames {
public:
    GeoNames();
    ~GeoNames();

    bool LoadData(const std::string& fileName, bool saveRaw = false);
    bool Init(std::ostream& err);

    bool Parse(std::vector<ParseResult>& results, const std::string& str, bool uniqueOnly = true) const;

private:
    class Impl;
    std::unique_ptr<Impl> Impl_;
};

} // namespace geonames
