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

#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "debug_settings.h"
#include <list>
#include <map>
#include <set>

namespace miosix {
    class MACRoundFactory;
    class MACContext;
    class MediumAccessController {
    public:
        MediumAccessController() = delete;
        MediumAccessController(const MediumAccessController& orig) = delete;
        static MediumAccessController& instance(const MACRoundFactory *const roundFactory, unsigned short panId, short txPower, unsigned int radioFrequency);
        inline unsigned int getRadioFrequency() const { return baseFrequency; }
        inline short getTxPower() const { return txPower; }
        inline unsigned short getPanId() const { return panId; }
        void run();
        //5 byte (4 preamble, 1 SFD) * 32us/byte
        static const int packetPreambleTime = 160000;
        //350us and forced receiverWindow=1 fails, keeping this at minimum
        //This is dependent on the optimization level, i usually use level -O2
        static const int receivingNodeWakeupAdvance = 450000;
        static const int sendingNodeWakeupAdvance = 500000; //500 us TODO fine tune, it was guessed
        static const int maxAdmittableReceivingWindow = 6000000; //6 ms
        inline int getOutgoingCount() { return sendQueue.size(); }
        /**
         * A method for asynchronically send a packet.
         * @param data the content to send. Data will be deleted after sending it, copy the data if needed.
         * @param dataSize
         * @param toNode
         * @param acked
         */
        void send(const void* data, int dataSize, unsigned short toNode, bool acked);
    private:
        MediumAccessController(const MACRoundFactory* const roundFactory, unsigned short panId, short txPower, unsigned int radioFrequency);
        virtual ~MediumAccessController();
        unsigned short panId;
        short txPower;
        unsigned int baseFrequency;
        MACContext* ctx;
        std::multimap<unsigned short, void*> sendQueue;
    };
}

#endif /* NETWORKMANAGER_H */

