#include <vector>
#include <fstream>
#include <codecvt>
#include <tclap/CmdLine.h>

#include "src/json.hpp"
#include "geonames/geonames.h"

using namespace std;

void Inc(nlohmann::json& stats, const string& name) {
    if (!stats.count(name)) {
        stats[name] = 0;
    }
    stats[name] = stats[name].get<size_t>() + 1;
}

void JsonResult(
    nlohmann::json& res,
    const string& name,
    const geonames::ParsedObject& obj,
    bool printInfo,
    bool printTokens
) {
    if (!obj) {
        return;
    }
    wstring_convert<codecvt_utf8<char32_t>, char32_t> utf8codec;
    res[name] = {
        { "name", utf8codec.to_bytes(obj.Object_->Name()) },
        { "latitude", obj.Object_->Latitude() },
        { "longitude", obj.Object_->Longitude() }
    };
    if (printInfo) {
        res[name]["id"] = obj.Object_->Id();
        res[name]["type"] = geonames::GeoTypeToString(obj.Object_->Type());
    }
    if (printTokens) {
        res["_" + name + "_tokens"] = obj.Tokens_;
    }
}

int Main(int argc, char* argv[]) {
    TCLAP::CmdLine cmd("Locate geonames in given strings");

    TCLAP::ValueArg<string> build("b", "build", "Build map file", false, "", "file_name", cmd);
    TCLAP::ValueArg<string> input("i", "input", "Input file", false, "", "file_name", cmd);
    TCLAP::MultiArg<string> query("q", "query", "Query string (discards -i)", false, "string", cmd);
    TCLAP::ValueArg<string> output("o", "output", "Output file", false, "", "file_name", cmd);
    TCLAP::ValueArg<string> jsonField("j", "json-field", "Input is json object per line, read given field", false, "", "field", cmd);
    TCLAP::ValueArg<string> extraDelimiters("", "extra-delimiters", "Extra set of characters to tokenize query", false, "", "field", cmd);
    TCLAP::ValueArg<string> defaultCountry("", "default-country", "Prefer given country", false, "", "field", cmd);
    TCLAP::ValueArg<double> mergeNear("m", "merge-near", "Merge nearby ambiguous results", false, 0, "haversine distance", cmd);
    TCLAP::SwitchArg uniqueOnly("u", "unique-only", "Output only results with unique match", cmd);
    TCLAP::SwitchArg queries("Q", "queries", "Add query string to result json", cmd);
    TCLAP::SwitchArg info("I", "info", "Add object info (id, type) to result json", cmd);
    TCLAP::SwitchArg tokens("T", "tokens", "Add tokens used to deduce objects to result json", cmd);
    TCLAP::SwitchArg parsed("P", "parsed", "Print only successfully parsed results", cmd);
    TCLAP::SwitchArg oneLine("1", "one-line", "Output result JSON in one line per request", cmd);
    TCLAP::SwitchArg printStats("S", "print-stats", "Print answer stats to stderr", cmd);
    TCLAP::UnlabeledValueArg<string> geodata("geodata", "Input map file or geonames data for -b", true, "", "file name", cmd);

    cmd.parse(argc, argv);

    geonames::GeoNames geoNames;

    ostringstream err;
    if (build.isSet()) {
        if (!geoNames.Build(build.getValue(), geodata.getValue(), err)) {
            cerr << "Failed to build map file: " << err.str() << endl;
            return 1;
        }
        cout << "Map file ready" << endl;
        return 0;
    }

    if (!geoNames.Init(geodata.getValue(), err)) {
        cerr << "Failed to initialize geodata: " << err.str() << endl;
        return 1;
    }

    auto* in = &cin;
    auto* out = &cout;

    unique_ptr<ifstream> inFile;
    unique_ptr<istringstream> inString;
    if (!input.getValue().empty()) {
        inFile.reset(new ifstream(input.getValue()));
        in = inFile.get();
    } else if (!query.getValue().empty()) {
        string queriesString;
        for (auto& q: query) {
            queriesString += q + '\n';
        }
        inString.reset(new istringstream(queriesString));
        in = inString.get();
    }
    unique_ptr<ofstream> outFile;
    if (!output.getValue().empty()) {
        outFile.reset(new ofstream(output.getValue()));
        out = outFile.get();
    }

    nlohmann::json stats;

    size_t n = 0;
    string line;
    geonames::ParserSettings settings;
    settings.MergeNear_ = mergeNear.getValue();
    settings.UniqueOnly_ = uniqueOnly.getValue();
    settings.Delimiters_ += extraDelimiters.getValue();
    settings.DefaultCountry_ = defaultCountry.getValue();
    vector<geonames::ParseResult> results;
    while (getline(*in, line)) {
        ++n;

        if (jsonField.isSet()) {
            nlohmann::json data;
            try {
                data = nlohmann::json::parse(line);
            } catch (const exception& e) {
                cerr << "Failed to parse JSON from line: " << n << " error: " << e.what() << endl;
                return 1;
            }
            auto it = data.find(jsonField.getValue());
            if (it != data.end()) {
                line = it.value().get<string>();
            } else {
                continue;
            }
        }

        nlohmann::json answer = { { "results", nlohmann::json::array() } };
        if (queries.getValue()) {
            answer["_query"] = line;
        }
        results.clear();
        if (geoNames.Parse(results, line, settings)) {
            for (auto& res: results) {
                nlohmann::json obj(nlohmann::json::object());
                obj["_score"] = res.Score_;
                JsonResult(obj, "country", res.Country_, info.getValue(), tokens.getValue());
                JsonResult(obj, "state", res.Province_, info.getValue(), tokens.getValue());
                JsonResult(obj, "city", res.City_, info.getValue(), tokens.getValue());
                answer["results"].push_back(obj);
            }
            Inc(stats, results.size() == 1 ? "unique" : "ambiguous");
        } else {
            Inc(stats, "unknown");
        }
        Inc(stats, "queries");

        if (!results.empty() || !parsed.getValue()) {
            *out << answer.dump(oneLine.getValue() ? -1 : 4) << endl;
        }
    }

    if (printStats.getValue()) {
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
