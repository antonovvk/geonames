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
    TCLAP::CmdLine cmd("Locate geonames in given strings");

    TCLAP::ValueArg<string> build("b", "build", "Build map file", false, "", "file_name", cmd);
    TCLAP::ValueArg<string> input("i", "input", "Input file", false, "", "file_name", cmd);
    TCLAP::MultiArg<string> queries("q", "query", "Query string (discards -i)", false, "string", cmd);
    TCLAP::ValueArg<string> output("o", "output", "Output file", false, "", "file_name", cmd);
    TCLAP::SwitchArg uniqueOnly("u", "unique-only", "Output only results with unique match", cmd);
    TCLAP::SwitchArg tokens("t", "tokens", "Output tokens used to deduce objects", cmd);
    TCLAP::SwitchArg oneLine("1", "one-line", "Output result JSON in one line per request", cmd);
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
    } else if (!queries.getValue().empty()) {
        string queriesString;
        for (auto& q: queries) {
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

    size_t n = 0;
    string line;
    vector<geonames::ParseResult> results;
    while (getline(*in, line)) {
        ++n;
        results.clear();
        if (geoNames.Parse(results, line, uniqueOnly.getValue())) {
            for (auto& res: results) {
                nlohmann::json obj(nlohmann::json::object());
                obj["score"] = res.Score_;
                JsonResult(obj, "country", res.Country_, tokens.getValue());
                JsonResult(obj, "state", res.Province_, tokens.getValue());
                JsonResult(obj, "city", res.City_, tokens.getValue());
                *out << obj.dump(oneLine.getValue() ? -1 : 4) << endl;
            }
        }
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
