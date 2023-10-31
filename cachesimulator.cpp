/*
Cache Simulator
Level one L1 and level two L2 cache parameters are read from file (block size, line per set and set per cache).
The 32 bit address is divided into tag bits (t), set index bits (s) and block offset bits (b)
s = log2(#sets)   b = log2(block size in bytes)  t=32-s-b
32 bit address (MSB -> LSB): TAG || SET || OFFSET

Tag Bits   : the tag field along with the valid bit is used to determine whether the block in the cache is valid or not.
Index Bits : the set index field is used to determine which set in the cache the block is stored in.
Offset Bits: the offset field is used to determine which byte in the block is being accessed.
*/

#include <algorithm>
#include <bitset>
#include <cmath>
#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <vector>
using namespace std;

// access state:
#define NA 0         // no action
#define RH 1         // read hit
#define RM 2         // read miss
#define WH 3         // Write hit
#define WM 4         // write miss
#define NOWRITEMEM 5 // no write to memory
#define WRITEMEM 6   // write to memory

int iter;
int Id;
struct config
{
    int L1blocksize;
    int L1setsize;
    int L1size;
    int L2blocksize;
    int L2setsize;
    int L2size;
};

/*********************************** ↓↓↓ Todo: Implement by you ↓↓↓ ******************************************/
// You need to define your cache class here, or design your own data structure for L1 and L2 cache

/*
A single cache block:
    - valid bit (is the data in the block valid?)
    - dirty bit (has the data in the block been modified by means of a write?)
    - tag (the tag bits of the address)
    - data (the actual data stored in the block, in our case, we don't need to store the data)
*/
#define DIRTY 2
#define VALID 1

struct CacheBlock
{
    // we don't actually need to allocate space for data, because we only need to simulate the cache action
    // or else it would have looked something like this: vector<number of bytes> Data;
    uint32_t tag{};
    uint16_t dirty{};
    uint16_t valid{};
};

/*
A CacheSet:
    - a vector of CacheBlocks
    - a counter to keep track of which block to evict next
*/
class CacheSet
{
    // tips:
    // Associativity: eg. resize to 4-ways set associative cache
    vector<CacheBlock> lines{};
    uint32_t capacity{};
    uint32_t idx{};

public:
    CacheSet() = default;
    CacheSet(uint32_t setSize) : capacity(setSize), lines(setSize)
    {
    }

    //
    // if contain, return {true, dirty bit}
    // else, return {false, ?}
    //
    pair<bool, uint8_t> contain(uint32_t tag, bool writeOp)
    {
        for (int i = 0; i < capacity; ++i)
        {
            if (lines[i].valid && lines[i].tag == tag)
            {
                if (writeOp)
                    lines[i].dirty = 1;
                return {true, lines[i].dirty};
            }
        }
        return {false, 0};
    }

    void invalid(uint32_t tag)
    {
        for (int i = 0; i < capacity; ++i)
        {
            if (lines[i].valid && lines[i].tag == tag)
            {
                lines[i].valid = 0;
                break;
            }
        }
    }

    //
    // Insert the data block with tag into the set.
    // the return value is a pair. The first value is the tag of the replaced block (if any),
    // and the second value is the significant and dirty bits of the replaced block (if dirty, | 2; if valid, | 1)
    //
    pair<uint32_t, uint8_t> evict(uint32_t tag, uint8_t dirty)
    {
        pair<uint32_t, uint8_t> res{};

        // search empty slot
        int i;
        for (i = 0;; ++i)
        {
            if (i == capacity)
            {
                i = idx;
                res.first = lines[idx].tag;
                res.second |= VALID;
                res.second |= 1 << lines[idx].dirty; // whether the block is dirty
                idx = (idx + 1) % capacity;

                break;
            }
            else if (!lines[i].valid)
                break;
        }

        lines[i].tag = tag;
        lines[i].dirty = dirty;
        lines[i].valid = 1;

        return res;
    }
};

// Tips: another example:
struct OpRes
{
    int L1AcceState, L2AcceState, MemAcceState;
};

class Cache
{
    vector<CacheSet> cacheSet{};
    uint32_t offset{};
    uint32_t setLen{};
    int id{};

    uint32_t getTag(uint32_t addr) const
    {
        return addr >> (offset + setLen);
    }

    uint32_t getSetIndex(uint32_t addr) const
    {
        return (addr >> offset) & ((1 << setLen) - 1);
    }

    pair<bool, uint8_t> access(uint32_t addr, bool writeOp)
    {
        // get the tag and index
        uint32_t tag = getTag(addr);
        uint32_t index = getSetIndex(addr);

        return cacheSet[index].contain(tag, writeOp);
    }

public:
    Cache() = default;
    Cache(uint32_t blockSize, uint32_t setSize, uint32_t cacheSize)
    {
        this->id = ++Id;
        // get the number of sets
        offset = log2(blockSize);
        if (setSize)
        {
            setLen = (10 + log2(cacheSize)) - log2(setSize) - offset;
            cacheSet.resize(1 << setLen, CacheSet(setSize));
        }
        else
        {
            setLen = 0;
            cacheSet.resize(1 << setLen, CacheSet(1024 * cacheSize / blockSize));
        }
    }

    pair<bool, uint8_t> read_access(uint32_t addr)
    {
        return access(addr, false);
    }

    pair<bool, uint8_t> write_access(uint32_t addr)
    {
        return access(addr, true);
    }

    void invalid(uint32_t addr)
    {
        // get the tag and index
        uint32_t tag = getTag(addr);
        uint32_t index = getSetIndex(addr);

        cacheSet[index].invalid(tag);
    }

    //
    // Insert the data block corresponding to addr into the set.
    // the return value is a pair. The first value is the address of the replaced block (if any),
    // and the second value is the significant and dirty bits of the replaced block (if dirty, | 2; if valid, | 1)
    //
    pair<uint32_t, uint8_t> evict(uint32_t addr, uint8_t dirty)
    {
        assert(!read_access(addr).first);
        // get the tag and index
        uint32_t tag = getTag(addr);
        uint32_t index = getSetIndex(addr);

        auto res = cacheSet[index].evict(tag, dirty);

        // If the replaced block is valid,
        // construct the address of the data block
        if (res.second & VALID)
        {
            // Tag | index | offset(00..)
            res.first <<= setLen;
            res.first |= index;
            res.first <<= offset;
        }
        return res;
    }
};

class CacheSystem
{
    Cache L1{};
    Cache L2{};

public:
    CacheSystem() = default;
    CacheSystem(struct config &cfg)
        : L1(cfg.L1blocksize, cfg.L1setsize, cfg.L1size), L2(cfg.L2blocksize, cfg.L2setsize, cfg.L2size)
    {
    }
    OpRes read(uint32_t addr)
    {
        OpRes res{};
        uint8_t dirty{};
        pair<bool, uint8_t> tmp;

        if (L1.read_access(addr).first) // L1 hit ?
            res = {RH, NA, NOWRITEMEM};
        else if ((tmp = L2.read_access(addr)).first) // L2 hit ?
        {
            dirty = tmp.second;
            res = {RM, RH, NOWRITEMEM};
            L2.invalid(addr); // set L2 valid low
        }
        else // Read from memory
            res = {RM, RM, NOWRITEMEM};

        // if read L1 miss, load from L2 or memory
        if (res.L1AcceState == RM)
        {
            auto victim = L1.evict(addr, dirty);
            // the victim is valid, place to L2(L2 definitely does not contain this data block)
            if (victim.second & VALID)
            {
                victim = L2.evict(victim.first, (victim.second & DIRTY) >> 1);
                if (victim.second == (DIRTY | VALID)) // the victim in L2 is valid & dirty, write to memory
                    res.MemAcceState = WRITEMEM;
            }
        }

        return res;
    }

    OpRes write(uint32_t addr)
    {
        OpRes res;

        if (L1.write_access(addr).first)
            res = {WH, NA, NOWRITEMEM};
        else if (L2.write_access(addr).first)
            res = {WM, WH, NOWRITEMEM};
        else
            res = {WM, WM, WRITEMEM};

        return res;
    }
};

/*********************************** ↑↑↑ Todo: Implement by you ↑↑↑ ******************************************/

int main(int argc, char *argv[])
{

    config cacheconfig;
    ifstream cache_params;
    string dummyLine;
    cache_params.open(argv[1]);
    while (!cache_params.eof()) // read config file
    {
        cache_params >> dummyLine;               // L1:
        cache_params >> cacheconfig.L1blocksize; // L1 Block size
        cache_params >> cacheconfig.L1setsize;   // L1 Associativity
        cache_params >> cacheconfig.L1size;      // L1 Cache Size
        cache_params >> dummyLine;               // L2:
        cache_params >> cacheconfig.L2blocksize; // L2 Block size
        cache_params >> cacheconfig.L2setsize;   // L2 Associativity
        cache_params >> cacheconfig.L2size;      // L2 Cache Size
    }
    ifstream traces;
    ofstream tracesout;
    string outname;
    outname = string(argv[2]) + ".out";
    traces.open(argv[2]);
    tracesout.open(outname.c_str());
    string line;
    string accesstype;     // the Read/Write access type from the memory trace;
    string xaddr;          // the address from the memory trace store in hex;
    unsigned int addr;     // the address from the memory trace store in unsigned int;
    bitset<32> accessaddr; // the address from the memory trace store in the bitset;

    /*********************************** ↓↓↓ Todo: Implement by you ↓↓↓ ******************************************/
    // Implement by you:
    // initialize the hirearch cache system with those configs
    // probably you may define a Cache class for L1 and L2, or any data structure you like
    if (cacheconfig.L1blocksize != cacheconfig.L2blocksize)
    {
        printf("please test with the same block size\n");
        return 1;
    }
    // cache c1(cacheconfig.L1blocksize, cacheconfig.L1setsize, cacheconfig.L1size,
    //          cacheconfig.L2blocksize, cacheconfig.L2setsize, cacheconfig.L2size);

    int L1AcceState = 0;  // L1 access state variable, can be one of NA, RH, RM, WH, WM;
    int L2AcceState = 0;  // L2 access state variable, can be one of NA, RH, RM, WH, WM;
    int MemAcceState = 0; // Main Memory access state variable, can be either NA or WH;

    CacheSystem cacheSystem(cacheconfig);
    OpRes res;
    if (traces.is_open() && tracesout.is_open())
    {
        while (getline(traces, line))
        {
            ++iter;
            // read mem access file and access Cache
            istringstream iss(line);
            if (!(iss >> accesstype >> xaddr))
            {
                break;
            }
            stringstream saddr(xaddr);
            saddr >> std::hex >> addr;
            accessaddr = bitset<32>(addr);

            // access the L1 and L2 Cache according to the trace;
            if (accesstype.compare("R") == 0) // a Read request
            {
                // Implement by you:
                //   read access to the L1 Cache,
                //   and then L2 (if required),
                //   update the access state variable;
                //   return: L1AcceState L2AcceState MemAcceState

                // For example:
                // L1AcceState = cache.readL1(addr); // read L1
                // if(L1AcceState == RM){
                //     L2AcceState, MemAcceState = cache.readL2(addr); // if L1 read miss, read L2
                // }
                // else{ ... }
                res = cacheSystem.read(accessaddr.to_ulong());
            }
            else
            { // a Write request
                // Implement by you:
                //   write access to the L1 Cache, or L2 / main MEM,
                //   update the access state variable;
                //   return: L1AcceState L2AcceState

                // For example:
                // L1AcceState = cache.writeL1(addr);
                // if (L1AcceState == WM){
                //     L2AcceState, MemAcceState = cache.writeL2(addr);
                // }
                // else if(){...}
                res = cacheSystem.write(accessaddr.to_ulong());
            }
            L1AcceState = res.L1AcceState;
            L2AcceState = res.L2AcceState;
            MemAcceState = res.MemAcceState;
            /*********************************** ↑↑↑ Todo: Implement by you ↑↑↑ ******************************************/

            // Grading: don't change the code below.
            // We will print your access state of each cycle to see if your simulator gives the same result as ours.
            tracesout << L1AcceState << " " << L2AcceState << " " << MemAcceState << endl; // Output hit/miss results for L1 and L2 to the output file;
        }
        traces.close();
        tracesout.close();
    }
    else
        cout << "Unable to open trace or traceout file ";

    return 0;
}
