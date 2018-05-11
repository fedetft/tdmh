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

namespace mxnet {
class MasterTimesyncDownlink : public TimesyncDownlink {
public:
    explicit MasterTimesyncDownlink(MACContext& ctx, long long firstTimesyncTime) :
            TimesyncDownlink(ctx, MacroStatus::IN_SYNC), slotframeTime(firstTimesyncTime) {
        auto panId = networkConfig.getPanId();
        packet = {{
                0x46, //frame type 0b110 (reserved), intra pan
                0x08, //no source addressing, short destination addressing
                0x00, //seq no reused as glossy hop count, 0=root node, it has to contain the source hop
                static_cast<unsigned char>(panId>>8),
                static_cast<unsigned char>(panId & 0xff), //destination pan ID
                0xff, 0xff,                               //destination addr (broadcast)
                0,0,0,0                                   //32bit timesync packet counter for absolute network time
            }};
    };
    
   /**
    * Used to increment the timesync packet counter used for slave nodes to
    * know the absolute network time
    */
    void incrementTimesyncPacketCounter()
    {
        auto ptr=reinterpret_cast<unsigned int*>(packet.data()+7);
        *ptr++;
    }
    
    MasterTimesyncDownlink() = delete;
    MasterTimesyncDownlink(const MasterTimesyncDownlink& orig) = delete;
    virtual ~MasterTimesyncDownlink() {};
    void execute(long long slotStart) override;
    std::pair<long long, long long> getWakeupAndTimeout(long long tExpected) override;
    long long getDelayToMaster() const override { return 0; }
    virtual long long getSlotframeStart() const { return slotframeTime; }

protected:
    
    void next() override;
    long long correct(long long int uncorrected) override;
private:
    long long slotframeTime;
};
}

