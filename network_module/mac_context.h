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
//#include "timesync/timesync_downlink.h"
#include "network_configuration.h"
#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/power_manager.h"
#include "stream/stream_manager.h"
#include "downlink_phase/timesync/networktime.h"
#include <functional>
#include <stdexcept>
#include <utility>
#ifdef CRYPTO
#include "crypto/key_management/key_manager.h"
#endif
// For thread synchronization
#ifdef _MIOSIX
#include <miosix.h>
#else
#include <mutex>
#endif

namespace mxnet {

class MediumAccessController;
class TimesyncDownlink;
class UplinkPhase;
class TopologyContext;
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

    /**
     * \param payloadBytes number of bytes of payload
     * \return the time in nanoseconds needed to transmit a packet with the
     * given number of payload bytes, assuming CRC is enabled
     */
    static long long radioTime(int payloadBytes)
    {
//      FIXME: replace radioStartup including receiving/sendingNodeWakeupAdvance and receiver window
        const long long radioStartup = 192000; //Time for the PLL to start
        const long long byteTime = 32000;      //The radio takes 32us to transmit each byte (250Kbit/s)
        const unsigned int overheadBytes = 8;  //4 byte preamble, sfd, length, 2 byte crc
        return radioStartup + (payloadBytes + overheadBytes) * byteTime;
    }

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
     * @return the ScheduleDistribution
     */
    ScheduleDownlinkPhase* const getScheduleDistribution() const { return scheduleDistribution; }

    /**
     * @return the DataPhase
     */
    DataPhase* const getDataPhase() const { return data; }

    /**
     * @return the StreamManager
     */
    StreamManager* getStreamManager() { return &streamMgr; }

    /**
     * @return the number of slots (of data slot size) in a generic tile
     */
    unsigned getSlotsInTileCount() const { return numSlotInTile; }

    /**
     * @return the slack time of each tile
     */
    unsigned long long getTileSlackTime() const { return tileSlackTime; }

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

    /**
     * @return the current tile from the protocol start, based on slotStart,
     * taking count that the computation for every slot happens before the slot starts.
     */
    unsigned int getCurrentTile(long long slotStart) const {
        //NOTE: slotStart is the LOCAL time (returned by miosix::getTime()) of
        //the beginning of a slot in a tile. Given that there may be some clock
        //synchronization error, we add half a slot length to make it more robust
        slotStart += (dataSlotDuration/2);
        auto tileDuration = networkConfig.getTileDuration();
        auto nt = NetworkTime::fromLocalTime(slotStart).get();
        return nt / tileDuration;
    }

    /**
     * @return the current tile from the protocol start, based on slotStart,
     * and the current slot in the current tile (from 0 to getSlotsInTileCount()-1).
     */
    std::pair<unsigned int, unsigned int> getCurrentTileAndSlot(NetworkTime nt) const {
        //NOTE: Given that there may be some clock
        //synchronization error, we add half a slot length to make it more robust
        unsigned long long time = nt.get() + dataSlotDuration/2;
        unsigned long long tileDuration = networkConfig.getTileDuration();
        unsigned int currentTile = time / tileDuration;
        unsigned long long timeInCurrentTile = time % tileDuration;
        unsigned int slotInCurrentTile = timeInCurrentTile / dataSlotDuration;
        return std::make_pair(currentTile, slotInCurrentTile);
    }

    /**
     * @return the number of timesyncs occured before tileCounter
     */
    unsigned int getNumTimesyncs(unsigned int tileCounter) const {
        int tilesPerTimesync = controlSuperframe.size() * networkConfig.getNumSuperframesPerClockSync();
        return (tileCounter + tilesPerTimesync - 1) / tilesPerTimesync;
    }

    void run();

    void stop();

    bool isReady();

    virtual void startScheduler() {};

    virtual void beginScheduling() {};

#ifdef CRYPTO

    virtual KeyManager* getKeyManager() { return keyMgr; }
#endif

protected:
    MACContext(const MediumAccessController& mac,
               miosix::Transceiver& transceiver,
               const NetworkConfiguration& config);
    
    void calculateDurations();

    TimesyncDownlink* timesync = nullptr;
    UplinkPhase* uplink = nullptr;
    ScheduleDownlinkPhase* scheduleDistribution = nullptr;
    DataPhase* data = nullptr;

    unsigned char hop = 0;
    const MediumAccessController& mac;
    const miosix::TransceiverConfiguration transceiverConfig;
    const NetworkConfiguration& networkConfig;
    unsigned short networkId;
    miosix::Transceiver& transceiver;
    miosix::PowerManager& pm;
    StreamManager streamMgr;
#ifdef CRYPTO
    KeyManager* keyMgr = nullptr;
#endif

    unsigned long long dataSlotDuration;
    unsigned long long downlinkSlotDuration;
    unsigned long long uplinkSlotDuration;
    unsigned long long tileSlackTime;

private:

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
    bool ready = false;
    /* Thread synchronization */
#ifdef _MIOSIX
    miosix::Mutex ready_mutex;
#else
    std::mutex ready_mutex;
#endif

};
}
