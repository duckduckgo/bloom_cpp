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

/*
 Bloom filter with djb2 and sdbm hashing. It is a loose C++ port of
 the js library at https://github.com/cry/jsbloom
 */
class BloomFilter {

public:
    BloomFilter(size_t maxItems, double targetProbability);

    BloomFilter(string importFilePath, size_t bits, size_t maxItems);

    BloomFilter(BinaryInputStream &in, size_t bits, size_t maxItems);

    void add(string element);

    bool contains(string element);

    void writeToFile(string exportFilePath);

    void writeToStream(BinaryOutputStream &out);

    size_t getBitCount();

private:
    size_t bits;
    vector<BlockType> bloomVector;
    size_t hashRounds;
};
