/***************************************************************************
 *   Copyright (C) 2022 by Luca Conterio                                   *
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

#include <miosix.h>
#include <thread>

using namespace miosix;

class TempSerialSensor {

public:
    TempSerialSensor() {}

    void run() {
        Thread::create(&TempSerialSensor::threadLauncher, 2048, MAIN_PRIORITY, this);
    }

    void sample() {
        while(true) {
            // scanf should only read 1 value and it 
            // returns the number of read values
            while(scanf("%d", &lastSample) != 1) { // consume input until finally the scanf only reads 1 value
                while(fgetc(stdin) != '\n'); // if it is not 1, consume the entire line
            }
            
            printf("[A] Sample: s=%d\n", lastSample);
        }
    }

    int getLastSample() { return lastSample; }

private:
    int lastSample = 0;

    static void threadLauncher(void* arg) {
        reinterpret_cast<TempSerialSensor*>(arg)->sample();
    }
};