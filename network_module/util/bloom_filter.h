/***************************************************************************
 *   Copyright (C) 2019 by Federico Terraneo                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#pragma once

#include <bitset>

namespace mxnet {

/**
 * A bloom filter using three Dietzfelbinger hash functions
 * (https://en.wikipedia.org/wiki/Universal_hashing).
 * Thanks to Nicholas Mainardi for the hash function suggestion.
 * \tparam T type to be held in the hash
 * \tparam N number of hash buckets, must be a power of 2. The memory occupied
 * by this bloom filter is N/8 bytes.
 */
template<typename T, unsigned N>
class BloomFilter
{
public:
    void emplace(const T& t);
    
    bool contains(const T& t) const;
    
    void clear() { bits.reset(); }
    
private:
    
    static unsigned hashDietzfelbinger(unsigned x, unsigned a);
    
    // Must be odd random numbers, taken from /dev/random
    static constexpr unsigned a1=0x68632e7d;
    static constexpr unsigned a2=0xf003cee3;
    static constexpr unsigned a3=0x677ac605;
    
    static constexpr unsigned log2(unsigned x, unsigned i)
    {   
        return x==1 ? i : log2(x>>1,i+1);
    }
    
    static constexpr unsigned M=log2(N,0);
    static_assert(N>=8,"BloomFilter: N >= 8");
    static_assert(N==(1<<M),"BloomFilter: N not power of 2");
    
    std::bitset<N> bits;
};

template<typename T, unsigned N>
unsigned BloomFilter<T,N>::hashDietzfelbinger(unsigned x, unsigned a)
{
    constexpr unsigned w=sizeof(unsigned)*8;
    return (a*x)>>(w-M);
}

template<typename T, unsigned N>
void BloomFilter<T,N>::emplace(const T& t)
{
    unsigned x=t; //TODO: support non-numeric T types
    bits[hashDietzfelbinger(x,a1)]=1;
    bits[hashDietzfelbinger(x,a2)]=1;
    bits[hashDietzfelbinger(x,a3)]=1;
}

template<typename T, unsigned N>
bool BloomFilter<T,N>::contains(const T& t) const
{
    unsigned x=t; //TODO: support non-numeric T types
    return bits[hashDietzfelbinger(x,a1)]
        && bits[hashDietzfelbinger(x,a2)]
        && bits[hashDietzfelbinger(x,a3)];
}

}

#ifdef BLOOM_TEST
#include <iostream>
using namespace std;
using namespace mxnet;

template<typename T, unsigned N>
void check(const BloomFilter<T,N>& bf, unsigned range)
{
    unsigned count=0;
    for(unsigned i=0;i<range;i++)
        if(bf.contains(i))
        {
            cout<<"["<<((i & 0xff00)>>8)<<" - "<<(i & 0xff)<<"] "<<i<<endl;
            count++;
        }
    cout<<"Total = "<<count<<endl;
}

void test1(unsigned fill=5, unsigned range=0xffff)
{
    BloomFilter<unsigned short,2048> bf;
    for(unsigned i=0;i<fill;i++) bf.emplace(i);
    check(bf,range);
}

void test2(unsigned range=0xffff)
{
    BloomFilter<unsigned short,2048> bf;
    //26 entries, topology @ 4236812 of 20191111_h18_night.txt
    bf.emplace(0x0001); // 0 - 1
    bf.emplace(0x0003); // 0 - 3
    bf.emplace(0x0004); // 0 - 4
    bf.emplace(0x0005); // 0 - 5
    bf.emplace(0x0007); // 0 - 7
    bf.emplace(0x000e); // 0 - 14
    bf.emplace(0x0103); // 1 - 3
    bf.emplace(0x0105); // 1 - 5
    bf.emplace(0x010e); // 1 - 14
    bf.emplace(0x0204); // 2 - 4
    bf.emplace(0x0208); // 2 - 8
    bf.emplace(0x0305); // 3 - 5
    bf.emplace(0x030e); // 3 - 14
    bf.emplace(0x0407); // 4 - 7
    bf.emplace(0x0408); // 4 - 8
    bf.emplace(0x0507); // 5 - 7
    bf.emplace(0x050d); // 5 - 13
    bf.emplace(0x0708); // 7 - 8
    bf.emplace(0x0709); // 7 - 9
    bf.emplace(0x070a); // 7 - 10
    bf.emplace(0x090a); // 9 - 10
    bf.emplace(0x090b); // 9 - 11
    bf.emplace(0x0a0b); // 10 - 11
    bf.emplace(0x0b0c); // 11 - 12
    bf.emplace(0x0c0d); // 12 - 13
    bf.emplace(0x0d0e); // 13 - 14
    check(bf,range);
}
#endif
