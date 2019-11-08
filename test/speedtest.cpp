#include <iostream>
#include <fstream>
#include <cstring>
#include <set>
#include <chrono>
#include <stdio.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <assert.h>
#include "BloomFilter.hpp"

using namespace std;

static const bool testBloomfilterEntries = true;
static const bool saveBloomStrings = false;

static const double TARGET_ERROR_RATE = 0.001;
static const double ACCEPTABLE_ERROR_RATE = TARGET_ERROR_RATE * 2.5;

static const unsigned MAX_ITEMS = 10000000;
static const unsigned int FILTER_ELEMENT_COUNT = 3000000;
static const unsigned int ADDITIONAL_TEST_DATA_ELEMENT_COUNT = 50000;


typedef std::chrono::high_resolution_clock::time_point timepoint;

static bool testBloomFilter(BloomFilter &bloomFilter, set<string> bloomData, set<string>testData);

static string createRandomString();

static set<string> createRandomStrings(unsigned int count);

static bool contains(set<string> set, const string &element);

static set<string> readStringsFromFile(const string &fileName);

static void writeStringsToFile(const set<string> &strings, const string &fileName);

timepoint get_now() {
 	return chrono::high_resolution_clock::now();
}

chrono::microseconds get_delta(timepoint pA, timepoint pB) {
	return std::chrono::duration_cast<chrono::microseconds>(pB - pA);
}


int main(void) {
    // 1. Create random stings
    auto t1 = get_now();
    BloomFilter bloomfilter1(MAX_ITEMS, TARGET_ERROR_RATE);
    set<string> bloomData = createRandomStrings(FILTER_ELEMENT_COUNT);
    set<string> testData = createRandomStrings(ADDITIONAL_TEST_DATA_ELEMENT_COUNT);
    testData.insert(bloomData.begin(), bloomData.end());
    printf("time: create strings %lld us\n", get_delta(t1, get_now()).count());

    if (saveBloomStrings) {
        writeStringsToFile(bloomData, "bloomStrings.txt");
    }

    // 2. Insert strings to bloom filter
    t1 = get_now();
    for (const auto &i : bloomData) {
        bloomfilter1.add(i);
    }
    uint64_t time_insert_us = get_delta(t1, get_now()).count();
    double per_insert_us = time_insert_us * 1.0 / bloomData.size();
    printf("time: insert blf %lld us, per insert %lf us, count %lu\n",
    time_insert_us, per_insert_us, bloomData.size());

    // 2. Time lookups
    t1 = get_now();
    for (const auto &i : testData) {
        bloomfilter1.contains(i);
    }
    uint64_t time_lookup_us = get_delta(t1, get_now()).count();
    double per_lookup_us = time_lookup_us * 1.0 / testData.size();
    printf("time: lookup blf %lld us, per lookup %lf us, count %lu\n",
    time_lookup_us, per_lookup_us, testData.size());

    // 4. Write it to file
    t1 = get_now();
    bloomfilter1.writeToFile("/tmp/bf1.bin");
    uint64_t time_write_us = get_delta(t1, get_now()).count();
    double per_write_us = time_write_us * 1.0 / bloomfilter1.sizeInBits;
    printf("time: write blf %lld us, per write %lf us, count %lu\n",
    time_write_us, per_write_us, bloomfilter1.sizeInBits);

    // 5. Read it back in
    t1 = get_now();
    BloomFilter bloomfilter2("/tmp/bf1.bin");
    uint64_t time_read_us = get_delta(t1, get_now()).count();
    double per_read_us = time_read_us * 1.0 / bloomfilter1.sizeInBits;
    printf("time: read blf %lld us, per read %lf us, count %lu\n",
    time_read_us, per_read_us, bloomfilter1.sizeInBits);

    // 6. write out 2nd bloomfilter
    bloomfilter2.writeToFile("/tmp/bf2.bin");

    // sleep a second to let write complete
    // sleep(1);

    // Diff the two bloom filter files to make sure they are identical
    int retval = system("diff /tmp/bf1.bin /tmp/bf2.bin");
    printf("Bloomfilter files match, diff %d\n", retval);
    assert(retval == 0);

    // 7. Check bloomfilters for correctness
    if (testBloomfilterEntries) {
        bool result;
        result = testBloomFilter(bloomfilter1, bloomData, testData);
        assert(result == true);
        printf("Original bloomfilter test passed\n");

        result = testBloomFilter(bloomfilter2, bloomData, testData);
        assert(result == true);
        printf("Reloaded bloomfilter test passed\n");
    }
}

static bool testBloomFilter(BloomFilter &bloomFilter, set<string> bloomData, set<string>testData) {
    unsigned int falsePositives = 0, truePositives = 0, falseNegatives = 0, trueNegatives = 0;
    auto t1 = get_now();
    for (const auto &element : testData) {
        bool result = bloomFilter.contains(element);
        if (contains(bloomData, element) && !result) falseNegatives++;
        if (!contains(bloomData, element) && result) falsePositives++;
        if (!contains(bloomData, element) && !result) trueNegatives++;
        if (contains(bloomData, element) && result) truePositives++;
    }
    uint64_t time_test_us = get_delta(t1, get_now()).count();
    printf("time: test blf %lf ms,\n", time_test_us / 1000.0 );

    double errorRate = (falsePositives + falseNegatives) / (double) testData.size();
    printf("falsePos: %u\n", falsePositives);
    printf("falseNeg: %u\n", falseNegatives);
    printf("truePos: %u\n", truePositives);
    printf("trueNeg: %u\n", trueNegatives);
    printf("errorRate: %lf\n", errorRate);

    assert(falseNegatives == 0);
    assert(truePositives == bloomData.size());
    assert(trueNegatives <= (testData.size() - bloomData.size()));
    assert(errorRate <= ACCEPTABLE_ERROR_RATE);
    return true;
}

static string createRandomString() {
    uuid_t id;
    uuid_generate(id);

    constexpr int MAX_UUID_UNPARSE_LENGTH = 37;
    char stringId[MAX_UUID_UNPARSE_LENGTH] = {};
    uuid_unparse(id, stringId);

    return stringId;
}

static set<string> createRandomStrings(unsigned int count) {
    set<string> set;
    for (unsigned int i = 0; i < count; i++) {
        set.insert(createRandomString());
    }
    return set;
}

static bool contains(set<string> set, const string &element) {
    return set.find(element) != set.end();
}

static set<string> readStringsFromFile(const string &fileName) {
    set<string> data;
    string line;
    ifstream file(fileName);
    while (getline(file, line)) {
        if (!line.empty()) {
            data.insert(line);
        }
    }
    return data;
}

static void writeStringsToFile(const set<string> &strings, const string &fileName) {
    ofstream file(fileName);
    for (const auto &element : strings) {
        file << element << endl;
    }
}
