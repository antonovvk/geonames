#pragma once

#include <string>
#include <memory>
#include <vector>
#include <iostream>

namespace geonames {

double HaversineDistance(double lat1d, double lon1d, double lat2d, double lon2d);

enum GeoType {
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

    _AreaRegion     = 62,
    _AreaRegionEcon = 64,
    _AreaRegionHist = 65,
    _AreaEnd        = 66,

    _TypesBegin = _PolitIndep,
    _TypesMain  = _PopulSect,
    _TypesEnd   = _AreaEnd
};

GeoType GeoTypeFromString(const std::string& str);
std::string GeoTypeToString(GeoType type);

class GeoObject {
protected:
    GeoObject()
    {
    }

public:
    virtual ~GeoObject()
    {
    }

    virtual uint32_t Id() const = 0;
    virtual GeoType Type() const = 0;
    virtual double Latitude() const = 0;
    virtual double Longitude() const = 0;
    virtual size_t Population() const = 0;

    virtual std::u32string Name() const = 0;
    virtual std::string AsciiName() const = 0;
    virtual std::string CountryCode() const = 0;
    virtual std::string ProvinceCode() const = 0;
    virtual std::vector<size_t> AltHashes() const = 0;

    bool IsCountry() const;
    bool IsProvince() const;
    bool IsCity() const;

    bool HasCountryCode() const;
    bool HasProvinceCode() const;

    double HaversineDistance(const GeoObject& obj) const;
};

typedef std::shared_ptr<GeoObject> GeoObjectPtr;

struct ParsedObject {
    GeoObjectPtr Object_;
    std::vector<std::string> Tokens_;

    operator bool() const {
        return Object_.get() != nullptr;
    }
};

struct ParseResult {
    ParsedObject Country_;
    ParsedObject Province_;
    ParsedObject City_;
    double Score_ = 0;
};

struct ParserSettings {
    std::string Delimiters_ = "\t .;,/&()–";
    std::string DefaultCountry_;
    bool UniqueOnly_ = false;
    double MergeNear_ = 0;
};

class GeoNames {
public:
    GeoNames();
    ~GeoNames();

    bool Build(const std::string& mapFileName, const std::string& rawFileName, std::ostream& err) const;
    bool Init(const std::string& mapFileName, std::ostream& err);

    bool Parse(std::vector<ParseResult>& results, const std::string& str, const ParserSettings& settings = ParserSettings()) const;

private:
    class Impl;
    std::unique_ptr<Impl> Impl_;
};

} // namespace geonames
