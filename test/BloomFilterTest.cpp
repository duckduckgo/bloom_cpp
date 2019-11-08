/*
 * Copyright (c) 2018 DuckDuckGo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <uuid/uuid.h>
#include <stdio.h>
#include <unistd.h>
#include "BloomFilter.hpp"

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

using namespace std;

static const unsigned int FILTER_ELEMENT_COUNT = 5000;
static const unsigned int ADDITIONAL_TEST_DATA_ELEMENT_COUNT = 5000;

static const double TARGET_ERROR_RATE = 0.001;
static const double ACCEPTABLE_ERROR_RATE = TARGET_ERROR_RATE * 2.5;

static void setup();
static set<string> createRandomStrings(unsigned int count);
static bool testBloomFilter(BloomFilter &bloomFilter, set<string> &bloomData, set<string> &testData);
static void writeStringsToFile(const set<string> &strings, const string &fileName);
static set<string> readStringsFromFile(const string &fileName);

// Global strings for creating and testing the bloom filters
static set<string> gBloomData;
static set<string> gTestData;

int main( int argc, char* argv[] ) {
  // global setup
  setup();
  // run tests
  int result = Catch::Session().run( argc, argv );
  // global clean-up - none
  return result;
}

static void setup() {
    // Create the global bloomfilter data
    BloomFilter bloomfilter1(FILTER_ELEMENT_COUNT, TARGET_ERROR_RATE);
    gBloomData = createRandomStrings(FILTER_ELEMENT_COUNT);
    gTestData = createRandomStrings(ADDITIONAL_TEST_DATA_ELEMENT_COUNT);
    gTestData.insert(gBloomData.begin(), gBloomData.end());

    // TODO use filesystem::temp_directory_path() to get platform independent tmp directory
    writeStringsToFile(gBloomData, "/tmp/bloomData.txt");
    writeStringsToFile(gTestData, "/tmp/testData.txt");
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

TEST_CASE("when BloomFilter is empty then contains is false") {
    BloomFilter testee(FILTER_ELEMENT_COUNT, TARGET_ERROR_RATE);
    REQUIRE_FALSE(testee.contains("abc"));
}

TEST_CASE("when BloomFilter contains element then contains is true") {
    BloomFilter testee(FILTER_ELEMENT_COUNT, TARGET_ERROR_RATE);
    testee.add("abc");
    REQUIRE(testee.contains("abc"));
}

TEST_CASE("when BloomFilter contains items then error is within range") {

    unsigned int falsePositives = 0, truePositives = 0, falseNegatives = 0, trueNegatives = 0;

    BloomFilter testee(FILTER_ELEMENT_COUNT, TARGET_ERROR_RATE);
    set<string> bloomData = createRandomStrings(FILTER_ELEMENT_COUNT);
    set<string> testData = createRandomStrings(ADDITIONAL_TEST_DATA_ELEMENT_COUNT);
    testData.insert(bloomData.begin(), bloomData.end());

    for (const auto &i : bloomData) {
        testee.add(i);
    }

    for (const auto &element : testData) {
        bool result = testee.contains(element);
        if (contains(bloomData, element) && !result) falseNegatives++;
        if (!contains(bloomData, element) && result) falsePositives++;
        if (!contains(bloomData, element) && !result) trueNegatives++;
        if (contains(bloomData, element) && result) truePositives++;
    }

    double errorRate = (falsePositives + falseNegatives) / (double) testData.size();
    REQUIRE(falseNegatives == 0);
    REQUIRE(truePositives == bloomData.size());
    REQUIRE(trueNegatives <= (testData.size() - bloomData.size()));
    REQUIRE(errorRate <= ACCEPTABLE_ERROR_RATE);
}

TEST_CASE("saving and reloading bloomfilters from disk produces correct results") {

    SECTION("when bloomfilter is saved and loaded and saved again the saved files match") {
        bool res;

        // 0. Create the bloom filter from the data and test it
        BloomFilter bloomfilter1(FILTER_ELEMENT_COUNT, TARGET_ERROR_RATE);
        for (const auto &i : gBloomData) {
            bloomfilter1.add(i);
        }
        res = testBloomFilter(bloomfilter1, gBloomData, gTestData);
        REQUIRE(res == true);

        // 1. Write it to disk
        bloomfilter1.writeToFile("/tmp/bf1.bin");

        // 2. Read it back into another bloom filter
        BloomFilter bloomfilter2("/tmp/bf1.bin");

        // 3. Write out 2nd bloomfilter
        bloomfilter2.writeToFile("/tmp/bf2.bin");

        // 4. Diff the two bloom filter files to make sure they are identical
        int retval = system("diff /tmp/bf1.bin /tmp/bf2.bin");
        REQUIRE(retval == 0);

        // 5. Test the second bloomfilter
        res = testBloomFilter(bloomfilter2, gBloomData, gTestData);
        REQUIRE(res == true);

        // 6. Verify values from the 2 bloomfilters match
        BloomSettings s1;
        BloomSettings s2;
        bool match;
        bloomfilter1.getBloomSettings(s1);
        bloomfilter2.getBloomSettings(s2);
        match = (s1.maxItems == s2.maxItems && s1.sizeInBits == s2.sizeInBits &&
                 s1.numBlocks == s2.numBlocks && s1.hashRounds == s2.hashRounds &&
                 s1.numInserted == s2.numInserted);
        REQUIRE(match);
    }

    SECTION("when bloomfilter is loaded from a stream handle the results are correct") {
        basic_ifstream<char> in("/tmp/bf2.bin", ifstream::binary);
        REQUIRE(in.good() == true);

        BloomFilter bloomfilter2(in);
        in.close();

        bool res = testBloomFilter(bloomfilter2, gBloomData, gTestData);
        REQUIRE(res == true);
    }
}

TEST_CASE("when bloomfilter is loaded from a saved legacy binary file the results are correct") {
    set<string> bloomData;
    // read in the legacy strings
    bloomData = readStringsFromFile("test/legacyBloomStrings.txt");
    REQUIRE(bloomData.size() == 1000);

    // add additional test data not in the bloomfilter
    set<string> testData = createRandomStrings(1000);
    testData.insert(bloomData.begin(), bloomData.end());

    // load the bloom filter from the legacy file
    BloomFilter bloomFilter("test/legacyBloomFilter.bin", bloomData.size());

    // test the bloom filter
    bool res = testBloomFilter(bloomFilter, bloomData, testData);
    REQUIRE(res == true);
}


static bool testBloomFilter(BloomFilter &bloomFilter, set<string> &bloomData, set<string> &testData) {
    unsigned int falsePositives = 0, truePositives = 0, falseNegatives = 0, trueNegatives = 0;
    for (const auto &element : testData) {
        bool result = bloomFilter.contains(element);
        if (contains(bloomData, element) && !result) falseNegatives++;
        if (!contains(bloomData, element) && result) falsePositives++;
        if (!contains(bloomData, element) && !result) trueNegatives++;
        if (contains(bloomData, element) && result) truePositives++;
    }

    double errorRate = (falsePositives + falseNegatives) / (double) testData.size();
    // printf("truePos: %u\n", truePositives);
    // printf("trueNeg: %u\n", trueNegatives);
    // printf("falsePos: %u\n", falsePositives);
    // printf("falseNeg: %u\n", falseNegatives);
    // printf("errorRate: %lf\n", errorRate);

    REQUIRE(falseNegatives == 0);
    REQUIRE(truePositives == bloomData.size());
    REQUIRE(trueNegatives <= (testData.size() - bloomData.size()));
    REQUIRE(errorRate <= ACCEPTABLE_ERROR_RATE);
    return true;
}

static set<string> readStringsFromFile(const string &fileName) {
    set<string> data;
    string line;
    ifstream file(fileName);
    REQUIRE(file.good() == true);

    while (getline(file, line)) {
        if (!line.empty()) {
            data.insert(line);
        }
    }
    file.close();
    return data;
}

static void writeStringsToFile(const set<string> &strings, const string &fileName) {
    ofstream file(fileName);
    for (const auto &element : strings) {
        file << element << endl;
    }
    file.close();
}
