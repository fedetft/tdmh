/***************************************************************************
 *   Copyright (C)  2017 by Terraneo Federico, Polidori Paolo              *
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

#include <utility>
#include <array>
#include "timesync_downlink.h"
#include "../../util/packet.h"

namespace mxnet {
class MasterTimesyncDownlink : public TimesyncDownlink {
public:
    explicit MasterTimesyncDownlink(MACContext& ctx);
    
    MasterTimesyncDownlink() = delete;
    MasterTimesyncDownlink(const MasterTimesyncDownlink& orig) = delete;

    void execute(long long slotStart) override;
    /**
     * Master node do not need resync since it never loses synchronization
     */
    void resync() override {}

    /**
     * Master node do not need desync since it never loses synchronization
     */
    void desync() override {}

    virtual long long getSlotframeStart() const override { return slotframeTime; }
    
    void macStartHook() override;

protected:
    /**
     * Set the timesync packet counter used for slave nodes to know the
     * absolute network time
     * \param value timesync packet counter
     */
    void setTimesyncPacketCounter(unsigned int value)
    {
        // Write the int (4 bytes) to the Packet
        *reinterpret_cast<unsigned int*>(&packet[7]) = value;
        packetCounter = value;
    }
    
    /**
     * Increment the timesync packet counter used for slave nodes to know the
     * absolute network time
     */
    void incrementTimesyncPacketCounter()
    {
        // Write the int (4 bytes) to the Packet
        packetCounter++;
        *reinterpret_cast<unsigned int*>(&packet[7]) = packetCounter;
    }
    
    void next() override;
    long long correct(long long int uncorrected) override;
private:
    long long slotframeTime;
    static const long long initializationDelay = 1000000;
    Packet packet;
};

}

