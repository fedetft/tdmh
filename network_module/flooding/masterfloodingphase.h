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

#ifndef MASTERFLOODINGPHASE_H
#define MASTERFLOODINGPHASE_H

#include "floodingphase.h"
#include <utility>
#include <array>

namespace miosix {
class MasterFloodingPhase : public FloodingPhase {
public:
    MasterFloodingPhase(const MediumAccessController& mac, long long startTime) :
            MasterFloodingPhase(startTime) {};
    MasterFloodingPhase() = delete;
    MasterFloodingPhase(const MasterFloodingPhase& orig) = delete;
    MasterFloodingPhase(long long startTime) :
            FloodingPhase(startTime) {};
    void execute(MACContext& ctx) override;
    virtual ~MasterFloodingPhase();
    
    inline virtual long long getNextRoundStart() override;
protected:
    inline std::array<unsigned char, syncPacketSize> getSyncPkt(int panId) {
        return {{
            0x46, //frame type 0b110 (reserved), intra pan
            0x08, //no source addressing, short destination addressing
            0x00, //seq no reused as glossy hop count, 0=root node, it has to contain the source hop
            static_cast<unsigned char>(panId>>8),
            static_cast<unsigned char>(panId & 0xff), //destination pan ID
            0xff, 0xff                                //destination addr (broadcast)
                }};
    }
private:
};
}

#endif /* MASTERFLOODINGPHASE_H */

