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

#include "miosix_utils_sim.h"
#include "NodeBase.h"
#include <string>

using namespace omnetpp;
namespace miosix {

void ledOn() {
    Leds::redOn = true;
    Leds::print();
}
void ledOff() {
    Leds::redOn = false;
    Leds::print();
}
long long getTime() { return simTime().inUnit(SIMTIME_NS); }

void Thread::nanoSleep(long long delta) {
    getNode()->waitAndDeletePackets(SimTime(delta, SIMTIME_NS));
}
void Thread::nanoSleepUntil(long long when) {
    getNode()->waitAndDeletePackets(SimTime(when, SIMTIME_NS) - simTime());
}

bool Leds::greenOn = false;
bool Leds::redOn = false;

void Leds::print(){
    using namespace std;
    EV_INFO << std::string(Leds::redOn? "\U0001F534": "\U000026AA") << " " << std::string(Leds::greenOn? "\U0001F535": "\U000026AA") << endl;
}

void greenLed::high() {
    Leds::greenOn = true;
    Leds::print();
}

void greenLed::low() {
    Leds::greenOn = false;
    Leds::print();
}

}
