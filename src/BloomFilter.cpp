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
#include <sys/stat.h>
#include "BloomFilter.hpp"


#define HDR_MAGIC1 0xBF
#define HDR_MAGIC2 0xAA
#define HDR_MAGIC3 0xFE
#define HDR_MAGIC4 0xED

#define VERSION_MAJOR 1
#define VERSION_MINOR 0

#define INLINE inline


// File Headers
typedef struct LegacyHeader_s {
    unsigned char e1;
    unsigned char e2;
    unsigned char e3;
    unsigned char e4;
} LegacyHeader;

typedef struct FileHeader_s {
    unsigned char version_major;
    unsigned char version_minor;
    unsigned char pad1[6];
    uint64_t sizeInBits;
    uint64_t maxItems;
    uint64_t hashRounds;
    uint64_t numInserted;
    unsigned char pad2[24];
} FileHeader;

static_assert(sizeof(LegacyHeader) == 4, "Check legacy header size");
static_assert(sizeof(FileHeader) == 64, "Check file header size");

// Forward declarations

static size_t calculateHashRounds(size_t size, size_t maxItems);

static unsigned int djb2Hash(string text);

static unsigned int sdbmHash(string text);

static unsigned int doubleHash(unsigned int hash1, unsigned int hash2, unsigned int round);

static void readFileHeader(BinaryInputStream &in, FileHeader *fileHdr);


// Implementation

BloomFilter::BloomFilter(size_t _maxItems, double targetProbability) : maxItems(_maxItems){
    sizeInBits = (size_t) ceil((maxItems * log(targetProbability)) / log(1.0 / (pow(2.0, log(2.0)))));
    bitsPerBlock = sizeof(BlockType) * 8;
    numBlocks = (sizeInBits + (bitsPerBlock-1)) / bitsPerBlock;
    bloomVector = vector<BlockType>(numBlocks);
    hashRounds = calculateHashRounds(sizeInBits, maxItems);
}

BloomFilter::BloomFilter(string importFilePath, size_t legacy_only_maxItems) {
    maxItems = legacy_only_maxItems;
    struct stat stat_buf;
    size_t filesize = 0;
    int rc = stat(importFilePath.c_str(), &stat_buf);
    if (rc != 0) {
        filesize = stat_buf.st_size;
    }
    basic_ifstream<BlockType> in(importFilePath, ifstream::binary);
    init(in, filesize);
    in.close();
}

BloomFilter::BloomFilter(BinaryInputStream &in, size_t legacy_only_maxItems) {
    maxItems = legacy_only_maxItems;
    init(in, 0);
}

void BloomFilter::init(BinaryInputStream &in, size_t streamSize) {
    size_t streamBytesRead;

    // Read in legacy header, check for magic vals indicating new format
    LegacyHeader legacyHdr;
    in.read((char *)&legacyHdr, sizeof(legacyHdr));
    streamBytesRead += sizeof(legacyHdr);
    if (! (legacyHdr.e1 == HDR_MAGIC1 && legacyHdr.e2 == HDR_MAGIC2 &&
           legacyHdr.e3 == HDR_MAGIC3 && legacyHdr.e4 == HDR_MAGIC4)) {
        // File doesn't match new format, assume legacy file format
        // TODO - log a message
        // printf("** Legacy bloomfilter file found **\n");
        const size_t component1 = legacyHdr.e1 << 0;
        const size_t component2 = legacyHdr.e2 << 8;
        const size_t component3 = legacyHdr.e3 << 16;
        const size_t component4 = legacyHdr.e4 << 24;
        const size_t elementCount = component1 + component2 + component3 + component4;
        sizeInBits = elementCount;
        hashRounds = calculateHashRounds(sizeInBits, maxItems);
    } else {
        // File matches newer file format
        FileHeader fileHdr;
        readFileHeader(in, &fileHdr);
        streamBytesRead += sizeof(fileHdr);
        assert(fileHdr.version_major == 1 && fileHdr.version_minor == 0);
        sizeInBits = fileHdr.sizeInBits;
        // Unnecessary to pass in maxItems when reloading from the new file format
        if (maxItems > 0 && fileHdr.maxItems != maxItems) {
            // TODO log a warning
            // printf("WARNING: maxItems parameter inconsistent with file header: %lu %llu",
            //        maxItems, fileHdr.maxItems);
        }
        maxItems = fileHdr.maxItems;

        size_t calcHashRounds;
        calcHashRounds = calculateHashRounds(sizeInBits, maxItems);
        if (fileHdr.hashRounds != calcHashRounds) {
            // TODO log a warning
            // printf("WARNING: hashRounds inconsistent with file header: %lu %lld",
            //       calcHashRounds, fileHdr.hashRounds);
        }
        hashRounds = fileHdr.hashRounds;
        numInserted = fileHdr.numInserted;

    }

    bitsPerBlock = sizeof(BlockType) * 8;
    numBlocks = (sizeInBits + (bitsPerBlock-1)) / bitsPerBlock;
    size_t bytesToRead = numBlocks * sizeof(BlockType);
    if (streamSize > 0) {
        // stream size information was provided
        if (bytesToRead != (streamSize - streamBytesRead)) {
            // TODO - throw an error?
            assert(bytesToRead == (streamSize - streamBytesRead));
        }
    }
    // Read in vector data
    bloomVector = vector<BlockType>(numBlocks);
    BlockType* blocks = bloomVector.data();
    in.read((char*)blocks, numBlocks * sizeof(BlockType));
}

static size_t calculateHashRounds(size_t size, size_t maxItems) {
    return (size_t) round(log(2.0) * size / maxItems);
}

void BloomFilter::add(string element) {
    unsigned int hash1 = djb2Hash(element);
    unsigned int hash2 = sdbmHash(element);

    for (size_t i = 0; i < hashRounds; i++) {
        unsigned int hash = doubleHash(hash1, hash2, i);
        size_t bitIndex = hash % sizeInBits;
        setBitAtIndex(bitIndex);
    }
    numInserted++;
}

bool BloomFilter::contains(string element) {
    unsigned int hash1 = djb2Hash(element);
    unsigned int hash2 = sdbmHash(element);

    for (size_t i = 0; i < hashRounds; i++) {
        unsigned int hash = doubleHash(hash1, hash2, i);
        size_t bitIndex = hash % sizeInBits;
        if (checkBitAtIndex(bitIndex) == false) {
            return false;
        }
    }
    return true;
}

INLINE void BloomFilter::setBitAtIndex(size_t bitIndex) {
    size_t blockIndex = bitIndex / bitsPerBlock;
    size_t blockOffset = bitIndex % bitsPerBlock;
    auto blk = bloomVector[blockIndex];
    bloomVector[blockIndex] = blk | (1 << blockOffset);
}

INLINE bool BloomFilter::checkBitAtIndex(size_t bitIndex) {
    size_t blockIndex = bitIndex / bitsPerBlock;
    size_t blockOffset = bitIndex % bitsPerBlock;
    auto blk = bloomVector[blockIndex];
    if ((blk & (1 << blockOffset)) == 0) {
        return false;
    }
    return true;
}

void BloomFilter::writeToFile(string path) {
    basic_ofstream<BlockType> out(path.c_str(), ofstream::binary);
    writeToStream(out);
    out.close();
}

void BloomFilter::writeToStream(BinaryOutputStream &out) {
    // Write out file headers
    LegacyHeader legacyHdr;
    memset(&legacyHdr, 0, sizeof(legacyHdr));
    legacyHdr.e1 = HDR_MAGIC1;
    legacyHdr.e2 = HDR_MAGIC2;
    legacyHdr.e3 = HDR_MAGIC3;
    legacyHdr.e4 = HDR_MAGIC4;
    out.write((char *)&legacyHdr, sizeof(legacyHdr));

    FileHeader fileHdr;
    memset(&fileHdr, 0, sizeof(fileHdr));
    fileHdr.version_major = VERSION_MAJOR;
    fileHdr.version_minor = VERSION_MINOR;
    // store ints in network byte order
    fileHdr.sizeInBits = htonll(sizeInBits);
    fileHdr.maxItems = htonll(maxItems);
    fileHdr.hashRounds = htonll(hashRounds);
    fileHdr.numInserted = htonll(numInserted);
    out.write((char *)&fileHdr, sizeof(fileHdr));

    // Write out bloom vector
    BlockType* blocks = bloomVector.data();
    out.write((char*)blocks, numBlocks * sizeof(BlockType));
}

void BloomFilter::getBloomSettings(BloomSettings &settings) {
    settings.maxItems = maxItems;
    settings.sizeInBits = sizeInBits;
    settings.bitsPerBlock = bitsPerBlock;
    settings.numBlocks = numBlocks;
    settings.hashRounds = hashRounds;
    settings.numInserted = numInserted;
}

static void readFileHeader(BinaryInputStream &in, FileHeader *fileHdr) {
    in.read((char *)fileHdr, sizeof(*fileHdr));
    // convert ints from network to host byte order
    fileHdr->sizeInBits = ntohll(fileHdr->sizeInBits);
    fileHdr->maxItems = ntohll(fileHdr->maxItems);
    fileHdr->hashRounds = ntohll(fileHdr->hashRounds);
    fileHdr->numInserted = ntohll(fileHdr->numInserted);
    return;
}

static INLINE unsigned int djb2Hash(string text) {
    unsigned int hash = 5381;
    for (char &iterator : text) {
        hash = ((hash << 5) + hash) + iterator;
    }
    return hash;
}

static INLINE unsigned int sdbmHash(string text) {
    unsigned int hash = 0;
    for (char &iterator : text) {
        hash = iterator + ((hash << 6) + (hash << 16) - hash);
    }
    return hash;
}

static INLINE unsigned int doubleHash(unsigned int hash1, unsigned int hash2, unsigned int round) {
    switch (round) {
        case 0:
            return hash1;
        case 1:
            return hash2;
        default:
            return (hash1 + (round * hash2) + (round ^ 2));
    }
}
