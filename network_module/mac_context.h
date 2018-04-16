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

#pragma once

//#include "uplink/topology/topology_context.h"
//#include "uplink/stream_management/stream_management_context.h"
//#include "timesync/timesync_downlink.h"
#include "network_configuration.h"
#include "interfaces-impl/transceiver.h"

namespace mxnet {
class MediumAccessController;
class TimesyncDownlink;
class UplinkPhase;
class TopologyContext;
class StreamManagementContext;
class ScheduleDownlinkPhase;
class DataPhase;
class MACContext {
public:
    MACContext() = delete;
    MACContext(const MACContext& orig) = delete;
    virtual ~MACContext() {};
    inline const MediumAccessController& getMediumAccessController() { return mac; }
    void setHop(unsigned char num) { hop = num; }
    unsigned char getHop() { return hop; }
    /**
     * Gets the default TransceiverConfiguration, which has the correct frequency and txPower,
     * crc enabled and non-strict timeout
     */
    const miosix::TransceiverConfiguration& getTransceiverConfig() { return transceiverConfig; }
    const miosix::TransceiverConfiguration getTransceiverConfig(bool crc, bool strictTimeout = false) {
        return miosix::TransceiverConfiguration(transceiverConfig.frequency, transceiverConfig.txPower, crc, strictTimeout);
    }
    const NetworkConfiguration& getNetworkConfig() const { return networkConfig; }
    unsigned short getNetworkId() const { return networkId; }

    void setNetworkId(unsigned short networkId) {
        if (!networkConfig.isDynamicNetworkId())
            throw std::runtime_error("Cannot dynamically set network id if not explicitly configured");
        this->networkId = networkId;
    }
    miosix::Transceiver& getTransceiver() { return transceiver; }

    TimesyncDownlink* const getTimesync() { return timesync; }
    UplinkPhase* const getUplink() { return uplink; }
    TopologyContext* getTopologyContext() const;
    StreamManagementContext* getStreamManagementContext() const;
    ScheduleDownlinkPhase* const getScheduleDownlink() { return schedule; }
    DataPhase* const getDataPhase() { return data; }
protected:
    MACContext(const MediumAccessController& mac, miosix::Transceiver& transceiver, const NetworkConfiguration& config, TimesyncDownlink* const timesync, UplinkPhase* const uplink);
private:
    unsigned short getDataslotCount();
    unsigned char hop;
    const MediumAccessController& mac;
    const miosix::TransceiverConfiguration transceiverConfig;
    const NetworkConfiguration& networkConfig;
    unsigned short networkId;
    miosix::Transceiver& transceiver;

    TimesyncDownlink* const timesync;
    UplinkPhase* const uplink;
    TopologyContext* topologyContext;
    StreamManagementContext* streamManagement;
    ScheduleDownlinkPhase* schedule;
    DataPhase* data;
};
}

