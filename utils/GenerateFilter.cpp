#include <fstream>
#include <iomanip>
#include <sstream>
#include <set>
#include <openssl/sha.h>

#include "BloomFilter.hpp"

using namespace std;


// Forward declarations

static set<string> readStringsFromFile(const string &fileName);

static void writeWhitelistToFile(const vector<string> &whitelistData, const string &fileName);

static string generateSha256(const string &fileName);

static string generateSpecification(size_t entries, double errorRate, const string &sha256);

static void replace(string &string, const std::string &fromString, const std::string &toString);


// Bloom generation script

int main(int argc, char *argv[], char *envp[]) {

    if (argc != 4) {
        cerr << "Usage: INPUT_FILE VALIDATION_FILE OUTPUT_FILES_PREFIX" << endl;
        return 1;
    }

    string bloomDataFile = argv[1];
    string validationDataFile = argv[2];
    string bloomOutputFile = string(argv[3]) + "-bloom.bin";
    string bloomSpecOutputFile = string(argv[3]) + "-bloom-spec.json";
    string whitelistOutputFile = string(argv[3]) + "-whitelist.json";
    double errorRate = 0.0001;

    cout << "Generating filter" << endl;
    set<string> bloomInput = readStringsFromFile(bloomDataFile);
    BloomFilter filter((int) bloomInput.size(), errorRate);
    for (const string &entry : bloomInput) {
        if (!entry.empty()) {
            filter.add(entry);
        }
    }
    filter.writeToFile(bloomOutputFile);

    cout << "Generating whitelist" << endl;
    set<string> validationData = readStringsFromFile(validationDataFile);
    vector<string> whitelistData;
    for (const string &entry : validationData) {
        bool isInFilter = bloomInput.find(entry) != bloomInput.end();
        if (filter.contains(entry) && !isInFilter) {
            whitelistData.push_back(entry);
        }
    }
    writeWhitelistToFile(whitelistData, whitelistOutputFile);

    cout << "Generating filter specification" << endl;
    string sha256 = generateSha256(bloomOutputFile);
    string spec = generateSpecification(bloomInput.size(), errorRate, sha256);
    ofstream specOutput(bloomSpecOutputFile);
    specOutput << spec;
}

static set<string> readStringsFromFile(const string &fileName) {

    set<string> data;

    string line;
    ifstream file(fileName);
    while (getline(file, line)) {
        data.insert(line);
    }

    return data;
}

static void writeWhitelistToFile(const vector<string> &whitelistData, const string &fileName) {
    ofstream file(fileName);
    file << "{ \"data\": [" << endl;

    for (int i = 0; i < whitelistData.size(); i++) {
        file << "\"" << whitelistData[i] << "\"";
        if (i < whitelistData.size() - 1) {
            file << ",";
        }
        file << endl;
    }

    file << "]}";
}

static string generateSha256(const string &fileName) {

    ifstream file(fileName);
    string fileContents((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, fileContents.c_str(), fileContents.size());
    SHA256_Final(hash, &sha256);

    stringstream ss;
    for (auto element : hash) {
        ss << hex << setw(2) << setfill('0') << (int) element;
    }

    return ss.str();
}

static string generateSpecification(size_t entries, double errorRate, const string &sha256) {

    string specification = R"({
        "totalEntries" : ENTRIES,
        "errorRate"    : ERROR,
        "sha256"       : "SHA256"
    })";

    replace(specification, "ENTRIES", to_string(entries));
    replace(specification, "ERROR", to_string(errorRate));
    replace(specification, "SHA256", sha256);

    return specification;
}

static void replace(string &string, const std::string &fromString, const std::string &toString) {
    size_t fromIndex = string.find(fromString);
    size_t toIndex = fromString.length();
    string.replace(fromIndex, toIndex, toString);
}
