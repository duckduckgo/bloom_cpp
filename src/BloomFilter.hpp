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
#include <string>
#include <vector>
#include <iterator>

using namespace std;

typedef char BlockType;
typedef basic_istream<BlockType> BinaryInputStream;
typedef basic_ostream<BlockType> BinaryOutputStream;

typedef struct BloomSettings_s {
    size_t maxItems;
    size_t sizeInBits;
    size_t bitsPerBlock;
    size_t numBlocks;
    size_t hashRounds;
    size_t numInserted;
} BloomSettings;

/*
 Bloom filter with djb2 and sdbm hashing. It is a loose C++ port of
 the js library at https://github.com/cry/jsbloom
 */
class BloomFilter {

public:
    BloomFilter(size_t _maxItems, double targetProbability);

    BloomFilter(string importFilePath, size_t legacy_only_maxItems = 0);

    BloomFilter(BinaryInputStream &in, size_t legacy_only_maxItems = 0);

    void add(string element);

    bool contains(string element);

    void writeToFile(string exportFilePath);

    void writeToStream(BinaryOutputStream &out);

    void getBloomSettings(BloomSettings &settings);

  private:
    void init(BinaryInputStream &in, size_t streamSize);

    void setBitAtIndex(size_t bitIndex);

    bool checkBitAtIndex(size_t bitIndex);

    size_t maxItems;
    size_t sizeInBits;
    size_t bitsPerBlock;
    size_t numBlocks;
    size_t hashRounds;
    size_t numInserted;
    vector<BlockType> bloomVector;
};
