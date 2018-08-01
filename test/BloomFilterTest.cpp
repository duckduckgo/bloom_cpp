#include <iostream>
#include <vector>
#include <uuid/uuid.h>
#include "BloomFilter.hpp"

#define CATCH_CONFIG_MAIN

#include "catch.hpp"


using namespace std;

static const unsigned int FILTER_ELEMENT_COUNT = 1000;
static const unsigned int ADDITIONAL_TEST_DATA_ELEMENT_COUNT = 9000;
static const double TARGET_FALSE_POSITIVE_RATE = 0.001;
static const double ACCEPTABLE_FALSE_POSITIVE_RATE = TARGET_FALSE_POSITIVE_RATE * 1.1;

static string createRandomString() {
    uuid_t id;
    uuid_generate(id);

    uuid_string_t stringId;
    uuid_unparse(id, stringId);

    return (char *) stringId;
}

static vector<string> createRandomStrings(unsigned int count) {
    vector<string> vector(count);
    for (int i = 0; i < count; i++) {
        vector[i] = createRandomString();
    }
    return vector;
}

static bool contains(vector<string> vector, string element) {
    return find(vector.begin(), vector.end(), element) != vector.end();
}

TEST_CASE("when BloomFilter is empty then contains is false") {
    BloomFilter testee(FILTER_ELEMENT_COUNT, TARGET_FALSE_POSITIVE_RATE);
    REQUIRE_FALSE(testee.contains("abc"));
}

TEST_CASE("when BloomFilter contains element then contains is true") {
    BloomFilter testee(FILTER_ELEMENT_COUNT, TARGET_FALSE_POSITIVE_RATE);
    testee.add("abc");
    REQUIRE(testee.contains("abc"));
}

TEST_CASE("when BloomFilter contains items then lookup results are within range") {

    unsigned int falsePositives = 0, truePositives = 0, falseNegatives = 0, trueNegatives = 0;

    BloomFilter testee(FILTER_ELEMENT_COUNT, TARGET_FALSE_POSITIVE_RATE);
    vector<string> bloomData = createRandomStrings(FILTER_ELEMENT_COUNT);
    vector<string> testData = createRandomStrings(ADDITIONAL_TEST_DATA_ELEMENT_COUNT);
    testData.insert(end(testData), begin(bloomData), end(bloomData));

    for (const auto &i : bloomData) {
        testee.add(i);
    }

    for (int i = 0; i < testData.size(); i++) {
        bool result = testee.contains(testData[i]);
        if (contains(bloomData, testData[i]) && !result) falseNegatives++;
        if (!contains(bloomData, testData[i]) && result) falsePositives++;
        if (!contains(bloomData, testData[i]) && !result) trueNegatives++;
        if (contains(bloomData, testData[i]) && result) truePositives++;
    }

    double falsePositiveRate = falsePositives / testData.size();
    REQUIRE(falseNegatives == 0);
    REQUIRE(truePositives == bloomData.size());
    REQUIRE(trueNegatives <= (testData.size() - bloomData.size()));
    REQUIRE(falsePositiveRate <= ACCEPTABLE_FALSE_POSITIVE_RATE);
}