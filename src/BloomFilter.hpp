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

    BloomFilter(string importFilePath, size_t maxItems);

    BloomFilter(BinaryInputStream &in, size_t maxItems);

    void add(string element);

    bool contains(string element);

    void writeToFile(string exportFilePath);

    void writeToStream(BinaryOutputStream &out);

private:
    size_t hashRounds;
    vector<bool> bloomVector;
};
