#include <iostream>
#include <string>
#include <vector>
#include <iterator>
#include <stdio.h>
#include <cstdio>
#include <fstream>
#include <math.h>
#include "BloomFilter.hpp"

using namespace std;

// Forward declarations

static unsigned int calculateHashRounds(unsigned int size, unsigned int maxItems);

static unsigned int djb2Hash(string text);

static unsigned int sdbmHash(string text);

static unsigned int doubleHash(unsigned int hash1, unsigned int hash2, unsigned int round);

static void writeVectorToStream(vector<bool>& bloomVector, BinaryOutputStream& out);

static template<typename T> T pack(const vector<bool>& filter, const size_t block, const size_t bits);

static vector<bool> readVectorFromFile(string path);

static vector<bool> readVectorFromStream(BinaryInputStream& in);

static void unpackIntoVector(vector<bool>& bloomVector,
                      const size_t offset,
                      const size_t bitsInThisBlock,
                      BinaryInputStream& in);


// Implementation

BloomFilter::BloomFilter(unsigned int maxItems, double targetProbability)
{
    unsigned int size = ceil((maxItems * log(targetProbability)) / log(1.0 / (pow(2.0, log(2.0)))));
    bloomVector = vector<bool>(size);
    hashRounds = calculateHashRounds(size, maxItems);
}

BloomFilter::BloomFilter(string importFilePath, unsigned int maxItems)
{
    bloomVector = readVectorFromFile(importFilePath);
    hashRounds = calculateHashRounds((unsigned int) bloomVector.size(), maxItems);
}

BloomFilter::BloomFilter(BinaryInputStream& in, unsigned int maxItems)
{
    bloomVector = readVectorFromStream(in);
    hashRounds = calculateHashRounds((unsigned int) bloomVector.size(), maxItems);
}

static unsigned int calculateHashRounds(unsigned int size, unsigned int maxItems)
{
    return round(log(2.0) * size / maxItems);
}

void BloomFilter::add(string element)
{
    unsigned int hash1 = djb2Hash(element);
    unsigned int hash2 = sdbmHash(element);
    
    for (int i = 0; i < hashRounds; i++)
    {
        unsigned int hash = doubleHash(hash1, hash2, i);
        unsigned int index = hash % bloomVector.size();
        bloomVector[index] = true;
    }
}

bool BloomFilter::contains(string element)
{
    unsigned int hash1 = djb2Hash(element);
    unsigned int hash2 = sdbmHash(element);
    
    for (int i = 0; i < hashRounds; i++)
    {
        unsigned int hash = doubleHash(hash1, hash2, i);
        unsigned int index = hash % bloomVector.size();
        if (!bloomVector[index])
        {
            return false;
        }
    }
    
    return true;
}

static unsigned int djb2Hash(string text)
{
    unsigned int hash = 5381;
    for (auto iterator = text.begin(); iterator != text.end(); iterator++)
    {
        hash = ((hash << 5) + hash) + *iterator;
    }
    return hash;
}

static unsigned int sdbmHash(string text)
{
    unsigned int hash = 0;
    for (auto iterator = text.begin(); iterator != text.end(); iterator++)
    {
        hash = *iterator + ((hash << 6) + (hash << 16) - hash);
    }
    return hash;
}

static unsigned int doubleHash(unsigned int hash1, unsigned int hash2, unsigned int round)
{
    switch (round)
    {
        case 0:
            return hash1;
        case 1:
            return hash2;
        default:
            return (hash1 + (round * hash2) + (round^2));
    }
}

void BloomFilter::writeToFile(string path)
{
    basic_ofstream<BlockType> out(path.c_str(), ofstream::binary);
    writeVectorToStream(bloomVector, out);
}


void BloomFilter::writeToStream(BinaryOutputStream& out)
{
    writeVectorToStream(bloomVector, out);
}

void writeVectorToStream(vector<bool>& bloomVector, BinaryOutputStream& out)
{
    const size_t elements = bloomVector.size();
    out.put(((elements & 0x000000ff) >> 0));
    out.put(((elements & 0x0000ff00) >> 8));
    out.put(((elements & 0x00ff0000) >> 16));
    out.put(((elements & 0xff000000) >> 24));
    
    const size_t bitsPerBlock = sizeof(BlockType) * 8;
    for (size_t i = 0; i < elements / bitsPerBlock; i++)
    {
        const BlockType buffer = pack<BlockType>(bloomVector, i, bitsPerBlock);
        out.put( buffer );
    }
    
    const size_t bitsInLastBlock = elements % bitsPerBlock;
    if (bitsInLastBlock > 0)
    {
        const size_t lastBlock = elements / bitsPerBlock;
        const BlockType buffer = pack<BlockType>(bloomVector, lastBlock, bitsInLastBlock);
        out.put( buffer );
    }
}

static template<typename T> T pack(const vector<bool>& filter, const size_t block, const size_t bits)
{
    const size_t sizeOfTInBits = sizeof(T) * 8;
    assert( bits <= sizeOfTInBits );
    T buffer = 0;
    for (int j = 0; j < bits; ++j)
    {
        const size_t offset = (block * sizeOfTInBits) + j;
        const T bit = filter[offset] << j;
        buffer |= bit;
    }
    return buffer;
}

static vector<bool> readVectorFromFile(string path)
{
    basic_ifstream<BlockType> inFile(path, ifstream::binary);
    return readVectorFromStream(inFile);
}

static vector<bool> readVectorFromStream(BinaryInputStream& in)
{
    const size_t component1 = in.get() << 0;
    const size_t component2 = in.get() << 8;
    const size_t component3 = in.get() << 16;
    const size_t component4 = in.get() << 24;
    const size_t elementCount = component1 + component2 + component3 + component4;
    
    vector<bool> bloomVector(elementCount);
    
    const size_t bitsPerBlock = sizeof(BlockType) * 8;
    const size_t fullBlocks = elementCount / bitsPerBlock;
    for (int i = 0; i < fullBlocks; ++i)
    {
        const size_t offset = i * bitsPerBlock;
        unpackIntoVector(bloomVector, offset, bitsPerBlock, in);
    }
    
    const size_t bitsInLastBlock = elementCount % bitsPerBlock;
    if (bitsInLastBlock > 0)
    {
        const size_t offset = bitsPerBlock * fullBlocks;
        unpackIntoVector(bloomVector, offset, bitsInLastBlock, in);
    }
    
    return bloomVector;
}

static void unpackIntoVector(vector<bool>& bloomVector,
                      const size_t offset,
                      const size_t bitsInThisBlock,
                      BinaryInputStream& in)
{
    const BlockType block = in.get();
    
    for (int j = 0; j < bitsInThisBlock; j++)
    {
        const BlockType mask = 1 << j;
        bloomVector[offset + j] = (block & mask) != 0;
    }
}
