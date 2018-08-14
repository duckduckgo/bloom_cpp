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

#include <cstdio>
#include <fstream>
#include <cmath>
#include <assert.h>
#include "BloomFilter.hpp"

// Forward declarations

static size_t calculateHashRounds(size_t size, size_t maxItems);

static unsigned int djb2Hash(string text);

static unsigned int sdbmHash(string text);

static unsigned int doubleHash(unsigned int hash1, unsigned int hash2, unsigned int round);

static void writeVectorToStream(vector<bool> &bloomVector, BinaryOutputStream &out);

static BlockType pack(const vector<bool> &filter, size_t block, size_t bits);

static vector<bool> readVectorFromFile(const string &path);

static vector<bool> readVectorFromStream(BinaryInputStream &in);

static void unpackIntoVector(vector<bool> &bloomVector,
                             size_t offset,
                             size_t bitsInThisBlock,
                             BinaryInputStream &in);


// Implementation

BloomFilter::BloomFilter(size_t maxItems, double targetProbability) {
    auto size = (size_t) ceil((maxItems * log(targetProbability)) / log(1.0 / (pow(2.0, log(2.0)))));
    bloomVector = vector<bool>(size);
    hashRounds = calculateHashRounds(size, maxItems);
}

BloomFilter::BloomFilter(string importFilePath, size_t maxItems) {
    bloomVector = readVectorFromFile(importFilePath);
    hashRounds = calculateHashRounds(bloomVector.size(), maxItems);
}

BloomFilter::BloomFilter(BinaryInputStream &in, size_t maxItems) {
    bloomVector = readVectorFromStream(in);
    hashRounds = calculateHashRounds(bloomVector.size(), maxItems);
}

static size_t calculateHashRounds(size_t size, size_t maxItems) {
    return (size_t) round(log(2.0) * size / maxItems);
}

void BloomFilter::add(string element) {
    unsigned int hash1 = djb2Hash(element);
    unsigned int hash2 = sdbmHash(element);

    for (int i = 0; i < hashRounds; i++) {
        unsigned int hash = doubleHash(hash1, hash2, i);
        size_t index = hash % bloomVector.size();
        bloomVector[index] = true;
    }
}

bool BloomFilter::contains(string element) {
    unsigned int hash1 = djb2Hash(element);
    unsigned int hash2 = sdbmHash(element);

    for (int i = 0; i < hashRounds; i++) {
        unsigned int hash = doubleHash(hash1, hash2, i);
        size_t index = hash % bloomVector.size();
        if (!bloomVector[index]) {
            return false;
        }
    }

    return true;
}

static unsigned int djb2Hash(string text) {
    unsigned int hash = 5381;
    for (char &iterator : text) {
        hash = ((hash << 5) + hash) + iterator;
    }
    return hash;
}

static unsigned int sdbmHash(string text) {
    unsigned int hash = 0;
    for (char &iterator : text) {
        hash = iterator + ((hash << 6) + (hash << 16) - hash);
    }
    return hash;
}

static unsigned int doubleHash(unsigned int hash1, unsigned int hash2, unsigned int round) {
    switch (round) {
        case 0:
            return hash1;
        case 1:
            return hash2;
        default:
            return (hash1 + (round * hash2) + (round ^ 2));
    }
}

void BloomFilter::writeToFile(string path) {
    basic_ofstream<BlockType> out(path.c_str(), ofstream::binary);
    writeVectorToStream(bloomVector, out);
}

void BloomFilter::writeToStream(BinaryOutputStream &out) {
    writeVectorToStream(bloomVector, out);
}

static void writeVectorToStream(vector<bool> &bloomVector, BinaryOutputStream &out) {

    const size_t elements = bloomVector.size();
    out.put(((elements & 0x000000ff) >> 0));
    out.put(((elements & 0x0000ff00) >> 8));
    out.put(((elements & 0x00ff0000) >> 16));
    out.put(((elements & 0xff000000) >> 24));

    const size_t bitsPerBlock = sizeof(BlockType) * 8;
    for (size_t i = 0; i < elements / bitsPerBlock; i++) {
        const BlockType buffer = pack(bloomVector, i, bitsPerBlock);
        out.put(buffer);
    }

    const size_t bitsInLastBlock = elements % bitsPerBlock;
    if (bitsInLastBlock > 0) {
        const size_t lastBlock = elements / bitsPerBlock;
        const BlockType buffer = pack(bloomVector, lastBlock, bitsInLastBlock);
        out.put(buffer);
    }
}

static BlockType pack(const vector<bool> &filter, size_t block, size_t bits) {

    const size_t sizeOfTInBits = sizeof(BlockType) * 8;
    assert(bits <= sizeOfTInBits);
    BlockType buffer = 0;
    for (int j = 0; j < bits; ++j) {
        const size_t offset = (block * sizeOfTInBits) + j;
        const BlockType bit = filter[offset] << j;
        buffer |= bit;
    }
    return buffer;
}

static vector<bool> readVectorFromFile(const string &path) {
    basic_ifstream<BlockType> inFile(path, ifstream::binary);
    return readVectorFromStream(inFile);
}

static vector<bool> readVectorFromStream(BinaryInputStream &in) {

    const size_t component1 = in.get() << 0;
    const size_t component2 = in.get() << 8;
    const size_t component3 = in.get() << 16;
    const size_t component4 = in.get() << 24;
    const size_t elementCount = component1 + component2 + component3 + component4;

    vector<bool> bloomVector(elementCount);
    const size_t bitsPerBlock = sizeof(BlockType) * 8;
    const size_t fullBlocks = elementCount / bitsPerBlock;
    for (int i = 0; i < fullBlocks; ++i) {
        const size_t offset = i * bitsPerBlock;
        unpackIntoVector(bloomVector, offset, bitsPerBlock, in);
    }

    const size_t bitsInLastBlock = elementCount % bitsPerBlock;
    if (bitsInLastBlock > 0) {
        const size_t offset = bitsPerBlock * fullBlocks;
        unpackIntoVector(bloomVector, offset, bitsInLastBlock, in);
    }

    return bloomVector;
}

static void unpackIntoVector(vector<bool> &bloomVector,
                             size_t offset,
                             size_t bitsInThisBlock,
                             BinaryInputStream &in) {

    const BlockType block = in.get();

    for (int j = 0; j < bitsInThisBlock; j++) {
        const BlockType mask = 1 << j;
        bloomVector[offset + j] = (block & mask) != 0;
    }
}
