/***************************************************************************
 *   Copyright (C)  2017 by Polidori Paolo              *
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

#include "interfaces-impl/transceiver.h"
#include "network_configuration.h"

namespace mxnet {
class MACContext;
class UplinkPhase;
class ScheduleDownlinkPhase;
class DataPhase;
class TimesyncDownlink;
class MediumAccessController {
public:
    MediumAccessController() = delete;
    MediumAccessController(const MediumAccessController& orig) = delete;
    virtual ~MediumAccessController();
    /**
     * The method for making the MAC protocol run and start being operative.
     */
    void run();
    //5 byte (4 preamble, 1 SFD) * 32us/byte
    static const unsigned int packetPreambleTime = 160000;
    //350us and forced receiverWindow=1 fails, keeping this at minimum
    //This is dependent on the optimization level, i usually use level -O2
    static const unsigned int maxPropagationDelay = 100;
    static const unsigned int receivingNodeWakeupAdvance = 450000;
    //Minimum ~550us, 200us of slack added
    static const unsigned int sendingNodeWakeupAdvance = 500000; //500 us TODO fine tune, it was guessed, used to be 750
    static const unsigned int maxAdmittableResyncReceivingWindow = 150000; //150 us
    static const unsigned char maxPktSize = 125;
    static const unsigned char maxPktSizeNoCRC = 127;
protected:
    MediumAccessController(MACContext* const ctx) : ctx(ctx) {};
    MACContext* const ctx;
};
}

