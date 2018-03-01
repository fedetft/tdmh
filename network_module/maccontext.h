/***************************************************************************
 *   Copyright (C)  2017 by Polidori Paolo                                 *
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

#ifndef MACCONTEXT_H
#define MACCONTEXT_H

#include "network_configuration.h"
#include "interfaces-impl/transceiver.h"
#include "topology_discovery/topology_context.h"
#include <type_traits>
#include <limits>

namespace miosix {
    class MACRound;
    class MACRoundFactory;
    class MediumAccessController;
    class SyncStatus;
    class SlotsNegotiator;
    class TopologyContext;
    class MACContext {
    public:
        MACContext() = delete;
        MACContext(const MACContext& orig) = delete;
        MACContext(const MACRoundFactory* const roundFactory, const MediumAccessController& mac, const NetworkConfiguration* const);
        virtual ~MACContext();
        MACRound* getCurrentRound() const { return currentRound; }
        MACRound* getNextRound() const { return nextRound; }
        MACRound* shiftRound();
        inline const MediumAccessController& getMediumAccessController() { return mac; }
        void setHop(unsigned char num) { hop = num; }
        unsigned char getHop() { return hop; }
        void setNextRound(MACRound* round);
        void initializeSyncStatus(SyncStatus* syncStatus);
        inline SyncStatus* getSyncStatus() { return syncStatus; }
        /**
         * Gets the default TransceiverConfiguration, which has the correct frequency and txPower,
         * crc enabled and non-strict timeout
         */
        inline const TransceiverConfiguration& getTransceiverConfig() { return transceiverConfig; }
        inline const TransceiverConfiguration getTransceiverConfig(bool crc, bool strictTimeout=false) {
            return TransceiverConfiguration(transceiverConfig.frequency, transceiverConfig.txPower, crc, strictTimeout);
        }
        inline const SlotsNegotiator* getSlotsNegotiator() { return slotsNegotiator; }
        inline const NetworkConfiguration* getNetworkConfig() const { return networkConfig; }
        inline TopologyContext* getTopologyContext() { return topologyContext; }

        unsigned short getNetworkId() const {
            return networkId;
        }

        void setNetworkId(unsigned short networkId) {
            if (!networkConfig->dynamicNetworkId)
                throw std::runtime_error("Cannot dynamically set network id if not explicitly configured");
            this->networkId = networkId;
        }

        static const unsigned short maxPacketSize = 127;
    private:
        unsigned char hop;
        const MediumAccessController& mac;
        SyncStatus* syncStatus;
        const TransceiverConfiguration transceiverConfig;
        const NetworkConfiguration* const networkConfig;
        TopologyContext* const topologyContext;
        MACRound* currentRound = nullptr;
        MACRound* nextRound = nullptr;
        SlotsNegotiator* slotsNegotiator;
        unsigned short networkId;
    };
}

#endif /* MACCONTEXT_H */

