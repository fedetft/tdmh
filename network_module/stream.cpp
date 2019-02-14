/***************************************************************************
 *   Copyright (C)  2018 by Federico Amedeo Izzo                           *
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

#include "stream.h"
#include "mac_context.h"

namespace mxnet {

Stream::Stream(MediumAccessController& tdmh, unsigned char dst,
               unsigned char dstPort, Period period, unsigned char payloadSize,
               Redundancy redundancy=Redundancy::NONE) : tdmh(tdmh) {
    // Save Stream parameters in SME
    MACContext* ctx = tdmh.getMACContext();
    streamMgr = ctx->getStreamManager();
    unsigned char src = ctx->getNetworkId();
    /* TODO: Implement srcPort, for the moment it is hardcoded to 0 */
    unsigned char srcPort = 0;
    sme = StreamManagementElement(src, dst, srcPort, dstPort, period, payloadSize, redundancy);
    // Register Stream to StreamManager (with thread synchronization)
    streamMgr->registerStream(sme, this);
}

}
