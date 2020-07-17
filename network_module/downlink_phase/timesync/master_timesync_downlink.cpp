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

#include "../../network_configuration.h"
#include "master_timesync_downlink.h"
#include "networktime.h"
#include "../../util/debug_settings.h"
#include "../../mac_context.h"
#include "../../crypto/aes_ocb.h"

using namespace miosix;

namespace mxnet {

MasterTimesyncDownlink::MasterTimesyncDownlink(MACContext& ctx) : TimesyncDownlink(ctx, MacroStatus::IN_SYNC)
{    
    auto panId = networkConfig.getPanId();
    unsigned char timesyncPkt[] = {
            0x46, //frame type 0b110 (reserved), intra pan
            0x08, //no source addressing, short destination addressing
            0x00, //seq no reused as glossy hop count, 0=root node, it has to contain the source hop
            static_cast<unsigned char>(panId>>8),
            static_cast<unsigned char>(panId & 0xff), //destination pan ID
            0xff, 0xff,                               //destination addr (broadcast)
            0,0,0,0                                   //32bit timesync packet counter for absolute network time
    };
    packet.put(&timesyncPkt, sizeof(timesyncPkt));

#ifdef CRYPTO
    unsigned int index = ctx.getKeyManager()->getMasterIndex();
    packet.put(&index, sizeof(index));
#endif
}

void MasterTimesyncDownlink::execute(long long slotStart)
{
    next();
    ctx.configureTransceiver(ctx.getTransceiverConfig());

#ifdef CRYPTO
    setPacketMasterIndex();
    if (ctx.getNetworkConfig().getAuthenticateControlMessages()) {
        packet.reserveTag();
        AesOcb& ocb = ctx.getKeyManager()->getTimesyncOCB();
        unsigned int tile = ctx.getCurrentTile(slotStart);
        /* Sequence number is always set to 1 in timesync messages.  */
        unsigned long long seqNo = 1;
        unsigned int mI = ctx.getKeyManager()->getMasterIndex();
        if (ENABLE_CRYPTO_TIMESYNC_DBG)
            print_dbg("[T] Authenticating timesync: tile=%u, seqNo=%llu, mI=%d\n", tile, seqNo, mI);
        ocb.setNonce(tile, seqNo, mI);
        packet.putTag(ocb);
    }
#endif

    //Sending synchronization start packet
    packet.send(ctx, slotframeTime);
    ctx.transceiverIdle();
    if (ENABLE_TIMESYNC_DL_INFO_DBG) {
        auto nt = NetworkTime::fromLocalTime(slotStart);
        print_dbg("[T] ST=%lld NT=%lld\n", slotframeTime, nt.get());
    }

#ifdef CRYPTO
    if (ctx.getNetworkConfig().getAuthenticateControlMessages()) {
        packet.discardTag();
    }
#endif
    
#ifdef _MIOSIX
    unsigned int stackSize = MemoryProfiling::getStackSize();
    unsigned int absFreeStack = MemoryProfiling::getAbsoluteFreeStack();
    print_dbg("[H] MAC stack %d/%d\n",stackSize-absFreeStack,stackSize);
#endif
}

void MasterTimesyncDownlink::macStartHook()
{
    slotframeTime = getTime() + initializationDelay;
    NetworkTime::setLocalNodeToNetworkTimeOffset(-slotframeTime);
    
    // Initialize considering the next() in execute
    slotframeTime -= networkConfig.getClockSyncPeriod();
    setTimesyncPacketCounter(-1);
}

void MasterTimesyncDownlink::next() {
    slotframeTime += networkConfig.getClockSyncPeriod();
    incrementTimesyncPacketCounter();
}

long long MasterTimesyncDownlink::correct(long long int uncorrected) {
    return uncorrected;
}

}
