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
    BloomFilter(unsigned int maxItems, double targetProbability);

    BloomFilter(string importFilePath, unsigned int maxItems);

    BloomFilter(BinaryInputStream &in, unsigned int maxItems);

    void add(string element);

    bool contains(string element);

    void writeToFile(string exportFilePath);

    void writeToStream(BinaryOutputStream &out);

private:
    unsigned int hashRounds;
    vector<bool> bloomVector;
};
