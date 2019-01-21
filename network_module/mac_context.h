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
#include "interfaces-impl/power_manager.h"
#include <functional>
#include <stdexcept>

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
    /**
     * @return a reference to the interface class.
     */
    const MediumAccessController& getMediumAccessController() { return mac; }

    /**
     * Sets the hop at which the node is. Used in the timesync phase.
     * @param num the hop count
     */
    void setHop(unsigned char num) { hop = num; }

    /**
     * @return the hop at which the node is.
     */
    unsigned char getHop() const { return hop; }

    /**
     * @return the default TransceiverConfiguration, which has the correct frequency and txPower,
     * crc enabled and non-strict timeout.
     */
    const miosix::TransceiverConfiguration& getTransceiverConfig() { return transceiverConfig; }

    /**
     * Returns a TransceiverConfiguration which uses the protocol channel and txPower.
     * @param crc if the CRC needs to be enabled.
     * @param strictTimeout if the function should return just after not having received the SFD
     * or for the whole packet duration.
     * @return a TransceiverConfiguration, which has the correct frequency and txPower.
     */
    const miosix::TransceiverConfiguration getTransceiverConfig(bool crc, bool strictTimeout = false) {
        return miosix::TransceiverConfiguration(transceiverConfig.frequency, transceiverConfig.txPower, crc, strictTimeout);
    }

    /**
     * @return the reference to the current configuration of the protocol.
     */
    const NetworkConfiguration& getNetworkConfig() const { return networkConfig; }

    /**
     * The network id, either statically configured or dynamically assigned.
     */
    unsigned short getNetworkId() const { return networkId; }

    /**
     * Sets the network id, if configured to be dynamic.
     */
    void setNetworkId(unsigned short networkId) {
        if (!networkConfig.isDynamicNetworkId())
            throw std::runtime_error("Cannot dynamically set network id if not explicitly configured");
        this->networkId = networkId;
    }

    /**
     * Configures the transceived with the given configuration
     */
    void configureTransceiver(miosix::TransceiverConfiguration cfg) { transceiver.configure(cfg); }

    /**
     * Sends a packet at a given time.
     * @param pkt the data to be sent.
     * @param size the size of the data, in bytes.
     * @param ns the time at which the data needs to be sent out, in nanoseconds.
     */
    void sendAt(const void *pkt, int size, long long ns);

    /**
     * Sends a packet at a given time.
     * @param pkt the data to be sent.
     * @param size the size of the data, in bytes.
     * @param ns the time at which the data needs to be sent out, in nanoseconds.
     * @param cbk the callback to be called in case of unhandled transceiver exceptions.
     */
    void sendAt(const void *pkt, int size, long long ns, std::function<void(std::exception&)> cbk);

    /**
     * Receives a packet until a given timeout
     * @param pkt the buffer in which the received data is put
     * @param size the dimension of such buffer, or the desired data size limit
     * @param timeout the timeout after which the function returns
     * @param c if the packet arrival timestamp needs to be corrected by FLOPSYNC-2 or not
     */
    miosix::RecvResult recv(void *pkt, int size, long long timeout, miosix::Transceiver::Correct c=miosix::Transceiver::Correct::CORR);

    /**
     * Receives a packet until a given timeout
     * @param pkt the buffer in which the received data is put
     * @param size the dimension of such buffer, or the desired data size limit
     * @param timeout the timeout after which the function returns
     * @param cbk the callback to be called in case of unhandled transceiver exceptions.
     * @param c if the packet arrival timestamp needs to be corrected by FLOPSYNC-2 or not
     */
    miosix::RecvResult recv(void *pkt, int size, long long timeout, std::function<void(std::exception&)> cbk, miosix::Transceiver::Correct c=miosix::Transceiver::Correct::CORR);

    /**
     * Puts the transceiver in idle state
     */
    inline void transceiverIdle() { transceiver.idle(); }

    inline void sleepUntil(long long wakeup) const
    {
        if(sleepDeep) pm.deepSleepUntil(wakeup);
        else miosix::Thread::nanoSleepUntil(wakeup);
    }

    /**
     * @return the TimesyncDownlink phase
     */
    TimesyncDownlink* const getTimesync() const { return timesync; }

    /**
     * @return the UplinkPhase
     */
    UplinkPhase* const getUplink() const { return uplink; }

    /**
     * @return the TopologyContext
     */
    TopologyContext* getTopologyContext() const;

    /**
     * @return the StreamManagementContext
     */
    StreamManagementContext* getStreamManagementContext() const;

    /**
     * @return the ScheduleDownlink
     */
    ScheduleDownlinkPhase* const getScheduleDownlink() const { return scheduleDistribution; }

    /**
     * @return the DataPhase
     */
    DataPhase* const getDataPhase() const { return data; }

    /**
     * @return the number of slots (of data slot size) in a generic tile
     */
    unsigned getSlotsInTileCount() const { return numSlotInTile; }

    /**
     * @return the number of data slots in an uplink tile
     */
    unsigned getDataSlotsInUplinkTileCount() const { return numDataSlotInUplinkTile; }

    /**
     * @return the number of data slots in a downlink tile
     */
    unsigned getDataSlotsInDownlinkTileCount() const { return numDataSlotInDownlinkTile; }

    /**
     * @return the number of data slots in a control superframe
     */
    unsigned getDataSlotsInControlSuperframeCount() const {
        return numDataSlotInUplinkTile * networkConfig.getControlSuperframeStructure().countUplinkSlots() +
                numDataSlotInDownlinkTile * networkConfig.getControlSuperframeStructure().countDownlinkSlots();
    }

    /**
     * @return the duration of a data slot
     */
    unsigned long long getDataSlotDuration() const { return dataSlotDuration; }

    /**
     * @return the duration of a downlink slot
     */
    unsigned long long getDownlinkSlotDuration() const { return downlinkSlotDuration; }

    /**
     * @return the duration of an uplink slot
     */
    unsigned long long getUplinkSlotDuration() const { return uplinkSlotDuration; }

    void run();

    void stop();

    virtual void startScheduler() {};

    virtual void beginScheduling() {};

protected:
    MACContext(const MediumAccessController& mac, miosix::Transceiver& transceiver, const NetworkConfiguration& config);
    
    void calculateDurations();

    TimesyncDownlink* timesync = nullptr;
    UplinkPhase* uplink = nullptr;
    ScheduleDownlinkPhase* scheduleDistribution = nullptr;
    DataPhase* data = nullptr;

    TopologyContext* topologyContext = nullptr;
    StreamManagementContext* streamManagement = nullptr;
    Stream* stream = nullptr;

    unsigned long long dataSlotDuration;
    unsigned long long downlinkSlotDuration;
    unsigned long long uplinkSlotDuration;
    unsigned long long tileSlackTime;
private:
    void warmUp();

    unsigned char hop;
    const MediumAccessController& mac;
    const miosix::TransceiverConfiguration transceiverConfig;
    const NetworkConfiguration& networkConfig;
    unsigned short networkId;
    miosix::Transceiver& transceiver;
    miosix::PowerManager& pm;
    ControlSuperframeStructure controlSuperframe;

    unsigned numSlotInTile;
    unsigned numDataSlotInDownlinkTile;
    unsigned numDataSlotInUplinkTile;

    //TODO write getters for the statistics
    unsigned sendTotal;
    unsigned sendErrors;
    unsigned rcvTotal;
    unsigned rcvErrors;

    volatile bool running;
    const bool sleepDeep=false; //TODO: make it configurable

};
}
