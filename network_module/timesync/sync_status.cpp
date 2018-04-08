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

#include "sync_status.h"

namespace mxnet {

unsigned char DynamicSyncStatus::missedPacket() {
    if(++missedPackets >= config->getMaxMissedTimesyncs()) {
        internalStatus = DESYNCHRONIZED;
        synchronizer->reset();
    } else {
        measuredFrameStart = computedFrameStart;
        std::pair<int,int> clockCorrectionReceiverWindow = synchronizer->lostPacket();
        clockCorrection = clockCorrectionReceiverWindow.first;
        receiverWindow = clockCorrectionReceiverWindow.second;
        updateVt();
    }
    return missedPackets;
}

void DynamicSyncStatus::receivedPacket(long long arrivalTime) {
    measuredFrameStart = arrivalTime;
    std::pair<int,int> clockCorrectionReceiverWindow = synchronizer->computeCorrection(arrivalTime - computedFrameStart);
    missedPackets = 0;
    clockCorrection = clockCorrectionReceiverWindow.first;
    receiverWindow = clockCorrectionReceiverWindow.second;
    updateVt();
}
}
