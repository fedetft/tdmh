/***************************************************************************
 *   Copyright (C)  2017 by Polidori Paolo                                 *
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

#include "../../network_module/bitwise_ops.h"
#include <array>
#include <algorithm>
#include <iostream>

#define CATCH_CONFIG_MAIN

#include "catch.hpp"

using namespace miosix;
using namespace std;

SCENARIO("bitwise populate memory areas containing 0101.. bits", "[bitalign]") {

	std::array<unsigned char, 16> pkt = {
		0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55
	};
	auto pkt1 = pkt;
	auto orig_pkt = pkt;

	GIVEN("Values already bit-aligned to the MSB") {

        WHEN("putting 101 from the third bit") {
            std::array<unsigned char, 1> val = {
                0xA0 //10100000
            };
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 3);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 2, 3, 0);

            THEN("B0: 01|101|101, B1..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6D); //01101101
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 1));
                it = pkt1.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6D); //01101101
                REQUIRE(std::equal(it, pkt1.end(), pkt1.begin() + 1));
            }
        }

		WHEN("the value fits exactly the first byte") {
			std::array<unsigned char, 1> val = {
                0xFF //11111111
            };
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 0, 8);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 0, 8, 0);

            THEN("B0: 11111111, B1..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == *val.begin()); //11111111
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 1));
                it = pkt1.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == *val.begin()); //11111111
                REQUIRE(std::equal(it, pkt1.end(), pkt1.begin() + 1));
            }
		}

		WHEN("the value fits 2 bytes") {
			std::array<unsigned char, 1> val = {
                0xAA //10101010
            };
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 8);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 2, 8, 0);

            THEN("B0: 01|101010, B1: 10|010101, B2..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6A); //01101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x95); //10010101
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 2));
                it = pkt1.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6A); //01101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x95); //10010101
                REQUIRE(std::equal(it, pkt1.end(), pkt1.begin() + 2));
            }
		}

		WHEN("the value fits 3 in bytes but is of 2") {
			std::array<unsigned char, 2> val = {
                0xAA, 0xAA //10101010 | 10101010
            };
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 16);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 2, 16, 0);

            THEN("B0: 01|101010, B1: 10101010, B2: 10|010101, B3..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6A); //01101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x95); //10010101
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 3));
                it = pkt1.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6A); //01101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x95); //10010101
                REQUIRE(std::equal(it, pkt1.end(), pkt1.begin() + 3));
            }
		}

		WHEN("the value fits 4 bytes but is of 3") {
			std::array<unsigned char, 3> val = {
                0xAA, 0xAA, 0xAA //10101010 | 10101010
            };
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 24);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 2, 24, 0);

            THEN("B0: 01|10101010, B1-2: 10101010, B3: 10|010101, B4-15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6A); //01101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x95); //10010101
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 4));
                it = pkt1.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6A); //01101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x95); //10010101
                REQUIRE(std::equal(it, pkt1.end(), pkt1.begin() + 4));
            }
		}

		WHEN("the value fits all bytes but is of all - 1") {
			std::array<unsigned char, 15> val = {
                0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA
            };
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 120);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 2, 120, 0);

            THEN("B0: 01|101010, B1-14: 10101010, B15: 10|010101") {
                REQUIRE(static_cast<unsigned short>(pkt[0]) == 0x6A); //01101010
				for (int i = 1; i < 15; i++)
					REQUIRE(static_cast<unsigned short>(pkt[i]) == 0xAA); //10101010
                REQUIRE(static_cast<unsigned short>(pkt[15]) == 0x95); //10010101
                REQUIRE(static_cast<unsigned short>(pkt1[0]) == 0x6A); //01101010
				for (int i = 1; i < 15; i++)
					REQUIRE(static_cast<unsigned short>(pkt1[i]) == 0xAA); //10101010
                REQUIRE(static_cast<unsigned short>(pkt1[15]) == 0x95); //10010101
            }
		}

		WHEN("the value fits all bytes and is of all, but starts at different points") {
			std::array<unsigned char, 16> val = {
                0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA
            };
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 126);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 2, 126, 0);
            THEN("B0: 01|101010, B1-15: 10101010") {
                REQUIRE(static_cast<unsigned short>(pkt[0]) == 0x6A); //01101010
				for (int i = 1; i < 16; i++)
					REQUIRE(static_cast<unsigned short>(pkt[i]) == 0xAA); //10101010
                REQUIRE(static_cast<unsigned short>(pkt1[0]) == 0x6A); //01101010
				for (int i = 1; i < 16; i++)
					REQUIRE(static_cast<unsigned short>(pkt1[i]) == 0xAA); //10101010
            }

			pkt1 = pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 8, 120);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 8, 120, 0);
            THEN("B0: 01010101, B1-15: 10101010") {
                REQUIRE(static_cast<unsigned short>(pkt[0]) == 0x55);
				for (int i = 1; i < 16; i++)
					REQUIRE(static_cast<unsigned short>(pkt[i]) == 0xAA); //10101010
                REQUIRE(static_cast<unsigned short>(pkt1[0]) == 0x55);
				for (int i = 1; i < 16; i++)
					REQUIRE(static_cast<unsigned short>(pkt1[i]) == 0xAA); //10101010
            }

			pkt1 = pkt = orig_pkt;
			array<unsigned char, 16> val1 = {
				0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55
			};	
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val1.data(), val1.size(), 13, 115);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val1.data(), val1.size(), 13, 115, 0);
            THEN("B0: 01010101, B1:01010|010, B1-15: 10101010") {
                REQUIRE(static_cast<unsigned short>(pkt[0]) == 0x55); //01101010
				REQUIRE(static_cast<unsigned short>(pkt[1]) == 0x52);
				for (int i = 2; i < 16; i++)
					REQUIRE(static_cast<unsigned short>(pkt[i]) == 0xAA); //10101010
                REQUIRE(static_cast<unsigned short>(pkt1[0]) == 0x55); //01101010
				REQUIRE(static_cast<unsigned short>(pkt1[1]) == 0x52);
				for (int i = 2; i < 16; i++)
					REQUIRE(static_cast<unsigned short>(pkt1[i]) == 0xAA); //10101010
            }
		}

		WHEN("the value fits 2 bytes and is of 1B, inserted from third byte, but end of 2nd and 3-4th bytes need to be set to 0") {
			std::array<unsigned char, 1> val = {
                0xAA //10101010
            };

            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 12);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 2, 12, 0);

            THEN("B0: 01|101010, B1: 10|0000|01, B2..15:01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6A); //01101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x81); //10000000
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 2));
                it = pkt1.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6A); //01101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x81); //10000000
                REQUIRE(std::equal(it, pkt1.end(), pkt1.begin() + 2));
            }

			pkt1 = pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 30);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 2, 30, 0);

            THEN("B0: 01|101010, B1: 10|000000, B2-3:0 B4..15:01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6A); //01101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x80); //10000000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 4));
                it = pkt1.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6A); //01101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x80); //10000000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
                REQUIRE(std::equal(it, pkt1.end(), pkt1.begin() + 4));
            }

			pkt1 = pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 34);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 2, 34, 0);

            THEN("B0: 01|101010, B1: 10|000000, B2-3:0 B4: 0000|0101, B5..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6A); //01101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x80); //10000000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 5);
                REQUIRE(std::equal(it, pkt.end() - 1, pkt.begin() + 5));
                it = pkt1.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6A); //01101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x80); //10000000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 5);
                REQUIRE(std::equal(it, pkt1.end() - 1, pkt1.begin() + 5));
            }
		}

		WHEN("the values is of 2B fit 3B starting from b4 but has limited length and needs to set 24b") {
			std::array<unsigned char, 2> val = {
                0xAA, 0xAA //10101010 | 10101010
            };
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 4, 20);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 4, 20, 0);

            THEN("B0: 0101|1010, B1: 10101010, B2:1010|0000, B3..15:01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x5A); //01011010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xA0); //10100000
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 3));
                it = pkt1.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x5A); //01011010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xA0); //10100000
                REQUIRE(std::equal(it, pkt1.end(), pkt1.begin() + 3));
            }

			pkt1 = pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 4, 24);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 4, 24, 0);

            THEN("B0: 0101|1010, B1: 10101010, B2:1010|0000, B3:0000|0101, B4..15:01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x5A); //01011010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xA0); //10100000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x05); //00000101
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 4));
                it = pkt1.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x5A); //01011010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xA0); //10100000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x05); //00000101
                REQUIRE(std::equal(it, pkt1.end(), pkt1.begin() + 4));
            }

			pkt1 = pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 4, 28);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 4, 28, 0);

            THEN("B0: 0101|1010, B1: 10101010, B2:1010|0000, B3:0, B4..15:01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x5A); //01011010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xA0); //10100000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 4));
                it = pkt1.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x5A); //01011010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xA0); //10100000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
                REQUIRE(std::equal(it, pkt1.end(), pkt1.begin() + 4));
            }

			pkt1 = pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 4, 40);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 4, 40, 0);

            THEN("B0: 0101|1010, B1: 10101010, B2:1010|0000, B3,4: 0, B5:0000|0101, B6..15:01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x5A); //01011010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xA0); //10100000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x05); //00000101
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 6));
                it = pkt1.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x5A); //01011010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xA0); //10100000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x05); //00000101
                REQUIRE(std::equal(it, pkt1.end(), pkt1.begin() + 6));
            }

			pkt1 = pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 4, 36);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 4, 36, 0);

            THEN("B0: 0101|1010, B1: 10101010, B2:1010|0000, B3,4: 0, B5..15:01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x5A); //01011010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xA0); //10100000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 5));
                it = pkt1.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x5A); //01011010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xA0); //10100000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
                REQUIRE(std::equal(it, pkt1.end(), pkt1.begin() + 5));
            }

			pkt1 = pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 4, 38);
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt1.data(), pkt1.size(), val.data(), val.size(), 4, 38, 0);

            THEN("B0: 0101|1010, B1: 10101010, B2:1010|0000, B3,4: 0, B5:00|010101, B6..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x5A); //01011010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xA0); //10100000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x15); //00010101
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 6));
                it = pkt1.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x5A); //01011010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xAA); //10101010
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xA0); //10100000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x15); //00010101
                REQUIRE(std::equal(it, pkt1.end(), pkt1.begin() + 6));
            }
		}
	};

    GIVEN("Values not aligned to the MSB") {
        std::array<unsigned char, 16> pkt = {
            0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55
        };
        auto orig_pkt = pkt;

        WHEN("adding a value which fits in 4 bits, right padded by 4 bits, to be put at position 1 (filling part of the first byte)") {
            std::array<unsigned char, 1> val = {
                0xFA //11111010
            };
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 4, 4);

            THEN("B0: 01|1010|01, B1..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 105); //01101001
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 1));
            }
        }

        WHEN("adding a value which fits in 4 bits, right padded by 4 bits, to be put at position 1 (filling part of the first byte), but of length higher than 4 (result needs zero right padding)") {
            std::array<unsigned char, 1> val = {
                0xFA //11111010
            };
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 6, 4);

            THEN("B0: 01|1010|00, B1..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 104); //01101000
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 1));
            }

			pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 8, 4);

            THEN("B0: 01|1010|00, B1: 00|010101, B2..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 104); //01101000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 21);//00010101 
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 2));
            }

			pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 17, 4);

            THEN("B0: 01|1010|00, B1: 0, B2: 000|10101, B2..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 104); //01101000
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 21);//00010101 
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 3));
            }
        }

        WHEN("Adding a value from its second byte") {
            std::array<unsigned char, 2> val = {
                0xFF, 0xAA //10101010
            };
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 6, 8);
			THEN("B0: 01|101010, B1..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 106); //01101010
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 1));
            }

			pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 8, 8);

            THEN("B0: 01|101010, B1: 10|010101, B2..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6A);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 149);//10010101 
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 2));
            }

			pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 17, 8);

            THEN("B0: 01|101010, B1: 10|000000, B2: 000|10101, B3..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6A);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 128);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x15); 
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 3));
            }

			pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 25, 8);

            THEN("B0: 01|101010, B1: 10|000000, B2: 0, B3: 000|10101, B4..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 0x6A);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 128);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x15); 
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 4));
            }
        }

        WHEN("Adding a value from the middle of its second byte") {
            std::array<unsigned char, 2> val = {
                0xFF, 0xFA //1010
            };
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 6, 12);
			THEN("B0: 01|1010|00, B1..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 104); //01101010
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 1));
            }

			pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 8, 12);

            THEN("B0: 01|1010|00, B1: 00|010101, B2..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 104);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x15);//10010101 
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 2));
            }

			pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 17, 12);

            THEN("B0: 01|1010|00, B1: 0, B2: 000|10101, B3..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 104);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x15); 
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 3));
            }

			pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 25, 12);

            THEN("B0: 01|1010|00, B1,2: 0, B3: 000|10101, B4..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 104);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x15); 
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 4));
            }
        }

        WHEN("Adding a value from the middle of its third byte") {
            std::array<unsigned char, 4> val = {
                0xFF, 0xFF, 0xFA, 0xAA
            };
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 6, 20);
			THEN("B0: 01|101010, B1..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 106); //01101010
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 1));
            }

			pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 8, 20);

            THEN("B0: 01|101010, B1: 10|010101, B2..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 106);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x95);//10010101 
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 2));
            }

			pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 17, 20);

            THEN("B0: 01|101010, B1: 101010|00, B2: 000|10101, B3..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 106);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xA8);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x15); 
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 3));
            }

			pkt = orig_pkt;
            BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(pkt.data(), pkt.size(), val.data(), val.size(), 2, 25, 20);

            THEN("B0: 01|101010, B1: 101010|00, B2: 0, B3: 000|10101, B4..15: 01010101") {
                auto it = pkt.begin();
                REQUIRE(static_cast<unsigned short>(*(it++)) == 106);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0xA8);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0);
				REQUIRE(static_cast<unsigned short>(*(it++)) == 0x15); 
                REQUIRE(std::equal(it, pkt.end(), pkt.begin() + 4));
            }
        }
    }
}
