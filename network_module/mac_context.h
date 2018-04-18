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
#include <functional>

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
    virtual ~MACContext();
    inline const MediumAccessController& getMediumAccessController() { return mac; }
    void setHop(unsigned char num) { hop = num; }
    unsigned char getHop() const { return hop; }
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
    void configureTransceiver(miosix::TransceiverConfiguration cfg) { transceiver.configure(cfg); }
    void sendAt(const void *pkt, int size, long long ns);
    void sendAt(const void *pkt, int size, long long ns, std::function<void(std::exception&)> cbk);
    miosix::RecvResult recv(void *pkt, int size, long long timeout, miosix::Transceiver::Correct c=miosix::Transceiver::Correct::CORR);
    miosix::RecvResult recv(void *pkt, int size, long long timeout, std::function<void(std::exception&)> cbk, miosix::Transceiver::Correct c=miosix::Transceiver::Correct::CORR);
    inline void transceiverTurnOn() { transceiver.turnOn(); }
    inline void transceiverTurnOff() { transceiver.turnOff(); }
    inline void transceiverIdle() { transceiver.idle(); }

    TimesyncDownlink* const getTimesync() const { return timesync; }
    UplinkPhase* const getUplink() const { return uplink; }
    TopologyContext* getTopologyContext() const;
    StreamManagementContext* getStreamManagementContext() const;
    ScheduleDownlinkPhase* const getScheduleDownlink() const { return schedule; }
    DataPhase* const getDataPhase() const { return data; }
protected:
    MACContext(const MediumAccessController& mac, miosix::Transceiver& transceiver, const NetworkConfiguration& config,
            TimesyncDownlink* const timesync, UplinkPhase* const uplink, StreamManagementContext* const smc,
            TopologyContext* const topology, ScheduleDownlinkPhase* const schedule);
private:
    unsigned short getDataslotCount() const;
    unsigned char hop;
    const MediumAccessController& mac;
    const miosix::TransceiverConfiguration transceiverConfig;
    const NetworkConfiguration& networkConfig;
    unsigned short networkId;
    miosix::Transceiver& transceiver;

    TimesyncDownlink* const timesync;
    UplinkPhase* const uplink;
    TopologyContext* const topologyContext;
    StreamManagementContext* const streamManagement;
    ScheduleDownlinkPhase* const schedule;
    DataPhase* const data;

    //TODO write getters for the statistics
    unsigned sendTotal;
    unsigned sendErrors;
    unsigned rcvTotal;
    unsigned rcvErrors;
};
}

