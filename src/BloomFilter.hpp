#include <iostream>
#include <string>
#include <vector>
#include <iterator>

typedef char BlockType;
typedef std::basic_istream<BlockType> BinaryInputStream;
typedef std::basic_ostream<BlockType> BinaryOutputStream;

/*
 Bloom filter with djb2 and sdbm hashing. It is a loose C++ port of
 the js library at https://github.com/cry/jsbloom
 */
class BloomFilter {

private:
    unsigned int hashRounds;
    std::vector<bool> bloomVector;
public:
    BloomFilter(unsigned int maxItems, double targetProbability);
    BloomFilter(std::string importFilePath, unsigned int maxItems);
    BloomFilter(BinaryInputStream& in, unsigned int maxItems);
    void add(std::string element);
    bool contains(std::string element);
    void writeToFile(std::string exportFilePath);
    void writeToStream(BinaryOutputStream& out);
};
