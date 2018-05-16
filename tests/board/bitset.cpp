/***************************************************************************
 *   Copyright (C)  2018 by Polidori Paolo                                 *
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

#include "../../network_module/uplink/topology/runtime_bitset.h"
#include <miosix.h>
#include <cstdio>
#include <cstdlib>

using namespace mxnet;

void test_result(const char* text, bool result) {
    iprintf("Test: ");
    iprintf(text);
    iprintf(result? "-> PASS\n": "-> FAIL\n");
}

void mem_read() {
    unsigned char _before[100];
    RuntimeBitset r(32, true);
    unsigned char _after[100];
    memset(_before, 0, 100);
    memset(_after, 0, 100);
    bool safe = true;
    for(int i = 0; i < 4; i++)
        safe &= r.data()[i] == 0xff;
    test_result("raw memory boundary is safe for reading", safe);
    safe = true;
    for (int i = 0; i < 32; i++)
        safe &= r[i];
    test_result("bit memory boundary is safe for reading", safe);
}

void mem_write() {
    unsigned char _before[100];
    RuntimeBitset r(32);
    unsigned char _after[100];
    memset(_before, 0, 100);
    memset(_after, 0, 100);
    for (int i = 0; i < 4; i++)
        r.data()[i] = static_cast<unsigned>(~0);
    bool safe = true;
    for(int i = 0; i < 100; i++)
        safe &= _before[i] == 0 && _after[i] == 0;
    test_result("raw memory boundary is safe for writing", safe);
    memset(_before, 0, 100);
    memset(_after, 0, 100);
    safe = true;
    for (int i = 0; i < 32; i++)
        r[i] = true;
    for(int i = 0; i < 100; i++)
        safe &= _before[i] == 0 && _after[i] == 0;
    test_result("bit memory boundary is safe for writing", safe);
}

void correct_values() {
    bool rawSafe = true;
    bool safe = true;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 8; j++) {
            RuntimeBitset r(32, false);
            uint8_t* ptr = r.data();
            r[(i << 3) + j] = true;
            for (int k = 0; k < 32; k++)
                safe &= r[k] == (k == (i << 3) + j);
            for (int k = 0; k < 4; k++)
                for (int l = 0; l < 8; l++)
                    rawSafe &= ((ptr[k] & (1 << l)) > 0) == (k == i && l == j);
        }
    test_result("values are written in the correct position", safe);
    test_result("values are written in the correct position in the raw data field", rawSafe);
}

void correct_copy() {
    RuntimeBitset a(256);
    for (int i = 0; i < 256; i++)
        a[i] = rand() % 2;
    RuntimeBitset b = a;
    bool safe = true;
    for (int i = 0; i < 256; i++)
        safe = b[i] == a[i];
    test_result("copy assignment", safe);
    RuntimeBitset c(a);
    safe = true;
    for (int i = 0; i < 256; i++)
        safe = c[i] == a[i];
    test_result("copy constructor", safe);
}

void correct_move() {
    bool vals[256];
    RuntimeBitset a(256);
    for (int i = 0; i < 256; i++) {
        a[i] = rand() % 2;
        vals[i] = a[i];
    }
    RuntimeBitset b(std::move(a));
    bool safe = a.data() == nullptr;
    for (int i = 0; i < 256; i++)
        safe = vals[i] == b[i];
    test_result("move constructor", safe);

    RuntimeBitset c(256);
    for (int i = 0; i < 256; i++) {
        c[i] = rand() % 2;
        vals[i] = c[i];
    }
    RuntimeBitset d = std::move(c);
    safe = c.data() == nullptr;
    for (int i = 0; i < 256; i++)
        safe = vals[i] == d[i];
    test_result("move assignment", safe);
}

int main() {
    mem_read();
    mem_write();
    correct_values();
    correct_copy();
    correct_move();
    /* TODO
     * correct_cast();
     * correct_full_init();
     * correct_comparisons();
     */
}
