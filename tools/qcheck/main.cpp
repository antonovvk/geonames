#include <vector>
#include <fstream>
#include <codecvt>
#include <tclap/CmdLine.h>

#include "src/json.hpp"
#include "geonames/geonames.h"

using namespace std;

void JsonResult(nlohmann::json& res, const string& name, const geonames::ParsedObject& obj, bool printTokens) {
    if (!obj) {
        return;
    }
    wstring_convert<codecvt_utf8<char32_t>, char32_t> utf8codec;
    res[name] = {
        { "name", utf8codec.to_bytes(obj.Object_->Name()) },
        { "latitude", obj.Object_->Latitude() },
        { "longitude", obj.Object_->Longitude() }
    };
    if (printTokens) {
        res[name + "_tokens"] = obj.Tokens_;
    }
}

int Main(int argc, char* argv[]) {
    TCLAP::CmdLine cmd("Geonames quality checker");

    TCLAP::ValueArg<string> input("i", "input", "Input file", false, "", "file_name", cmd);
    TCLAP::MultiArg<string> queries("q", "query", "Query string (discards -i)", false, "string", cmd);
    TCLAP::ValueArg<string> jsonField("j", "json-field", "Input is json object per line, read given field", true, "", "field", cmd);
    TCLAP::ValueArg<string> output("o", "output", "Output file", false, "", "file_name", cmd);
    TCLAP::ValueArg<string> jsonUpdate("", "json-update", "Input file is json object per line, add result as field obj", false, "", "field", cmd);
    TCLAP::ValueArg<double> mergeNear("m", "merge-near", "Merge nearby ambiguous results", false, 0, "haversine distance", cmd);
    TCLAP::SwitchArg uniqueOnly("u", "unique-only", "Output only results with unique match", cmd);
    TCLAP::SwitchArg tokens("t", "tokens", "Output tokens used to deduce objects", cmd);
    TCLAP::SwitchArg oneLine("1", "one-line", "Output result JSON in one line per request", cmd);
    TCLAP::SwitchArg compareResults("", "compare-results", "Used with --json-update. Extract position from existing object and compare", cmd);
    TCLAP::ValueArg<double> epsilon("e", "epsilon", "Report errors if distance more than epsilon", false, 0.1, "number", cmd);
    TCLAP::UnlabeledValueArg<string> geodata("geodata", "Input map file", true, "", "file name", cmd);

    cmd.parse(argc, argv);

    geonames::GeoNames geoNames;
    ostringstream err;
    if (!geoNames.Init(geodata.getValue(), err)) {
        cerr << "Failed to initialize geodata: " << err.str() << endl;
        return 1;
    }

    auto* in = &cin;
    auto* out = &cout;

    unique_ptr<ifstream> inFile;
    unique_ptr<istringstream> inString;
    if (input.isSet()) {
        inFile.reset(new ifstream(input.getValue()));
        in = inFile.get();
    } else if (queries.isSet()) {
        string queriesString;
        for (auto& q: queries) {
            queriesString += q + '\n';
        }
        inString.reset(new istringstream(queriesString));
        in = inString.get();
    }
    unique_ptr<ofstream> outFile;
    if (output.isSet()) {
        outFile.reset(new ofstream(output.getValue()));
        out = outFile.get();
    }

    wstring_convert<codecvt_utf8<char32_t>, char32_t> utf8codec;

    bool cmpResults = compareResults.getValue() && jsonUpdate.isSet();
    size_t total = 0;
    size_t cmp_matched = 0;
    size_t cmp_errors = 0;
    size_t cmp_missing = 0;
    size_t cmp_ambiguous = 0;
    size_t unique = 0;
    size_t missing = 0;
    size_t ambiguous = 0;

    size_t n = 0;
    string line;
    geonames::ParserSettings settings;
    settings.MergeNear_ = mergeNear.getValue();
    settings.UniqueOnly_ = uniqueOnly.getValue();
    vector<geonames::ParseResult> results;
    while (getline(*in, line)) {
        ++n;
        if (!jsonField.getValue().empty()) {
            nlohmann::json data;
            try {
                data = nlohmann::json::parse(line);
            } catch (const exception& e) {
                cerr << "Failed to parse JSON from line: " << n << " error: " << e.what() << endl;
                return 1;
            }
            const nlohmann::json* old = nullptr;
            if (cmpResults) {
                auto it = data.find(jsonUpdate.getValue());
                if (it != data.end()) {
                    old = &it.value();
                    if (!old->count("lat") || !old->count("lng")) {
                        old = nullptr;
                    }
                }
            }
            nlohmann::json res;

            auto it = data.find(jsonField.getValue());
            if (it != data.end()) {
                results.clear();
                if (geoNames.Parse(results, it.value().get<string>(), settings)) {
                    assert(!results.empty());
                    string city;
                    string state;
                    string country;
                    double lat;
                    double lng;
                    if (results[0].City_) {
                        city = utf8codec.to_bytes(results[0].City_.Object_->Name());
                        lat = results[0].City_.Object_->Latitude();
                        lng = results[0].City_.Object_->Longitude();
                    } else if (results[0].Province_) {
                        state = utf8codec.to_bytes(results[0].Province_.Object_->Name());
                        lat = results[0].Province_.Object_->Latitude();
                        lng = results[0].Province_.Object_->Longitude();
                    } else {
                        assert(results[0].Country_);
                        country = utf8codec.to_bytes(results[0].Country_.Object_->Name());
                        lat = results[0].Country_.Object_->Latitude();
                        lng = results[0].Country_.Object_->Longitude();
                    }
                    res = {
                        { "city", city },
                        { "state", state },
                        { "country", country },
                        { "lat", lat },
                        { "lng", lng }
                    };
                    if (old) {
                        if (results.size() == 1) {
                            const double e = sqrt(
                                pow(lat - old->find("lat").value().get<double>(), 2) +
                                pow(lng - old->find("lng").value().get<double>(), 2)
                            );
                            if (e > epsilon.getValue()) {
                                ++cmp_errors;
                                cerr << "Data: " << it.value().get<string>() << endl;
                                cerr << old->dump(4) << endl;
                                nlohmann::json obj(nlohmann::json::object());
                                JsonResult(obj, "country", results[0].Country_, tokens.getValue());
                                JsonResult(obj, "state", results[0].Province_, tokens.getValue());
                                JsonResult(obj, "city", results[0].City_, tokens.getValue());
                                cerr << obj.dump(4) << endl;
                            } else {
                                ++cmp_matched;
                            }
                        } else {
                            ++cmp_ambiguous;
                        }
                    } else if (results.size() == 1) {
                        ++unique;
                    } else {
                        ++ambiguous;
                    }
                } else if (old) {
                    ++cmp_missing;
                } else {
                    ++missing;
                }
                ++total;
            }
            if (jsonUpdate.getValue().empty()) {
                data = res;
            } else if (!old && !res.is_null()) {
                data[jsonUpdate.getValue()] = res;
            }
            *out << data.dump(oneLine.getValue() ? -1 : 4) << endl;
        }
    }

    if (cmpResults) {
        nlohmann::json stats = {
            { "total", total },
            { "cmp_matched", cmp_matched },
            { "cmp_errors", cmp_errors },
            { "cmp_missing", cmp_missing },
            { "cmp_ambiguous", cmp_ambiguous },
            { "unique", unique },
            { "missing", missing },
            { "ambiguous", ambiguous },
            { "valid_stats", total == (
                cmp_matched +
                cmp_errors +
                cmp_missing +
                cmp_ambiguous +
                unique +
                missing +
                ambiguous
            ) }
        };
        cerr << stats.dump(4) << endl;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    try {
        return Main(argc, argv);
    } catch (const TCLAP::ArgException& e) {
        cerr << "error: " << e.error() << " for arg " << e.argId() << endl;
    } catch (const exception& e) {
        cerr << "Caught exception: " << e.what() << endl;
    }
    return 1;
}
