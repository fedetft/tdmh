/***************************************************************************
 *   Copyright (C) 2018-2019 by Federico Amedeo Izzo and Federico Terraneo *
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

#include "dynamic_schedule_distribution.h"
#include "../data_phase/dataphase.h"
#include "../tdmh.h"
#include "../util/packet.h"
#include "../util/debug_settings.h"

using namespace miosix;

namespace mxnet {

void DynamicScheduleDownlinkPhase::execute(long long slotStart)
{
    bool shouldNotListen = handleActivationAndRekeying(slotStart);
    if(shouldNotListen) return;

    Packet pkt;
    // Receive the schedule packet
    if(recvPkt(slotStart, pkt))
    {
        // Parse the schedule packet
        SchedulePacket spkt(panId);
        spkt.deserialize(pkt);
        newHeader = spkt.getHeader();
        if(ENABLE_SCHEDULE_DIST_DYN_INFO_DBG)
            printHeader(newHeader);
        if(newHeader.isSchedulePacket() &&
           newHeader.getActivationTile()<=ctx.getCurrentTile(slotStart)) {
            if(ENABLE_SCHEDULE_DIST_DBG)
                print_dbg("[SD] BUG: schedule activation tile in the past\n");
        }
        
        //Packet received
        switch(status)
        {
            case ScheduleDownlinkStatus::APPLIED_SCHEDULE:
            {
                if(newHeader.isSchedulePacket()) {
                    initSchedule(spkt);
                    status = ScheduleDownlinkStatus::SENDING_SCHEDULE;
                } else applyInfoElements(spkt);
                break;
            }
            case ScheduleDownlinkStatus::SENDING_SCHEDULE:
            {
                /* NOTE: checks for exiting this state are taken care of
                 * before trying to receive packets */
                if(newHeader.isSchedulePacket()) {
                    if(newHeader.getScheduleID() != header.getScheduleID()) {
                        initSchedule(spkt);
                    } else {
                        appendToSchedule(spkt);
                        currentSendingRound++;
                    }
                }
                break;
            }
            case ScheduleDownlinkStatus::INCOMPLETE_SCHEDULE:
            {
                if(newHeader.isSchedulePacket()) {
                    initSchedule(spkt);
                    status = ScheduleDownlinkStatus::SENDING_SCHEDULE;
                } else {
                    applyInfoElements(spkt);
                    handleIncompleteSchedule();
                }
                break;
            }
            default:
                assert(false);
        }
    } else {
        
        //No packet received
        if(status == ScheduleDownlinkStatus::INCOMPLETE_SCHEDULE) {
            handleIncompleteSchedule();
        } else if (status == ScheduleDownlinkStatus::SENDING_SCHEDULE) {
            /* Increment counter even if the packet was missed. */
            currentSendingRound++;
        }
    }
//     print_dbg("! %lld\n",getTime()-slotStart);
}

void DynamicScheduleDownlinkPhase::advance(long long slotStart)
{
    handleActivationAndRekeying(slotStart);
}

void DynamicScheduleDownlinkPhase::desync()
{
    //NOTE: only resetting our internal state, and not applying the empty
    //schedule to the rest of the MAC. The rest of the MAC will clear its
    //schedule when their desync() is called
    //TODO: check that dataPhase and streamManager DO CLEAR their schedule
    header = ScheduleHeader();
    schedule.clear();
    received.clear();
    incompleteScheduleCounter = 0;
    
    status = ScheduleDownlinkStatus::APPLIED_SCHEDULE;
}

bool DynamicScheduleDownlinkPhase::handleActivationAndRekeying(long long slotStart)
{
    /**
     * Handle slots in which we don't need to receive packets: rekeying slots
     * and activation tile
     */
    bool ret = false;
    if(status == ScheduleDownlinkStatus::SENDING_SCHEDULE) {
        /* Check if we are ready so stop listening and start rekeying. If not,
         * do nothing here and try to receive a packet. */
        if(currentSendingRound >= sendingRounds) {
            if(isScheduleComplete()) {
                setNewSchedule(slotStart);
                if(ENABLE_SCHEDULE_DIST_DBG)
                    print_dbg("[SD] full schedule %s\n",scheduleStatusAsString().c_str());
            } else {
#ifdef CRYPTO
                /* If the schedule received is incomplete, we still have to
                 * perform rekeying on the key manager to take care of the
                 * phase keys */
                ctx.getKeyManager()->startRekeying();
                if (ENABLE_CRYPTO_REKEYING_DBG) {
                    unsigned int currentTile = ctx.getCurrentTile(slotStart);
                    print_dbg("[SD] N=%d start rekeying at tile %d\n",
                              myId, currentTile);
                }
#endif
                resetAndDisableSchedule(slotStart);
            }
            status = ScheduleDownlinkStatus::REKEYING;
            ret = true;
        }
    } else if (status == ScheduleDownlinkStatus::REKEYING) {
        unsigned int currentTile = ctx.getCurrentTile(slotStart);
        if(currentTile >= header.getActivationTile()) {
            if(isScheduleComplete()) {
                applyNewSchedule(slotStart);
                status = ScheduleDownlinkStatus::APPLIED_SCHEDULE;
            } else {
#ifdef CRYPTO
                if (ENABLE_CRYPTO_REKEYING_DBG) {
                    print_dbg("[SD] N=%d apply rekeying at tile %d\n",
                              myId, currentTile);
                }
                ctx.getKeyManager()->applyRekeying();
#endif
                status = ScheduleDownlinkStatus::INCOMPLETE_SCHEDULE;
            }
        } else {
#ifdef CRYPTO
            if(isScheduleComplete()) streamMgr->continueRekeying();
#endif
        }
        /* We never listen while in this state */
        ret = true;
    }
    return ret;
}
bool DynamicScheduleDownlinkPhase::recvPkt(long long slotStart, Packet& pkt)
{
    bool received;
#if FLOOD_TYPE == 0
    auto arrivalTime = slotStart + (ctx.getHop() - 1) * rebroadcastInterval;
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    auto rcvResult = pkt.recv(ctx, arrivalTime);
    ctx.transceiverIdle(); //Save power waiting for rebroadcast time
    // Received a valid schedule packet
    if(rcvResult.error == RecvResult::ErrorCode::OK && pkt.checkPanHeader(panId) == true)
    {
        // Retransmit the schedule packet unless you belong to maximum hop
        if(ctx.getHop() < ctx.getNetworkConfig().getMaxHops()) {
            ctx.configureTransceiver(ctx.getTransceiverConfig());
            pkt.send(ctx, rcvResult.timestamp + rebroadcastInterval);
            ctx.transceiverIdle();
        }
        received = true;
    } else received = false;
#elif FLOOD_TYPE == 1
    Packet tempPkt;
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    int maxHop = ctx.getNetworkConfig().getMaxHops();
    bool send = false; //Flood receiver, first time, receive
    bool receivedAtLeastOnce = false;
    for(int slot = 0; slot < maxHop; slot++)
    {
        if(send)
        {
            tempPkt.send(ctx, slotStart);
            slotStart += rebroadcastInterval;
            send = false; //Sent: next time, receive
        } else {
            auto rcvResult = tempPkt.recv(ctx, slotStart);
            if(rcvResult.error == RecvResult::ErrorCode::OK && tempPkt.checkPanHeader(panId) == true)
            {
                slotStart = rcvResult.timestamp + rebroadcastInterval;
                pkt = tempPkt;
                receivedAtLeastOnce = true;
                send = true; //Received successfully: next time, send
            } else slotStart += rebroadcastInterval;
        }
        
    }
    ctx.transceiverIdle();
    received = receivedAtLeastOnce;
#else
#error
#endif //FLOOD_TYPE

#ifdef CRYPTO
    if(ctx.getNetworkConfig().getAuthenticateControlMessages()) {
        if(received) {
            AesGcm& gcm = ctx.getKeyManager()->getScheduleDistributionGCM();
            unsigned int tileNumber = ctx.getCurrentTile(slotStart);
            unsigned int seqNo = 1;
            unsigned int masterIndex = ctx.getKeyManager()->getMasterIndex();
            if(ENABLE_CRYPTO_DOWNLINK_DBG)
                print_dbg("[SD] Verifying downlink: tile=%d, seqNo=%d, mI=%d\n",
                          tileNumber, seqNo, masterIndex);
            gcm.setIV(tileNumber, seqNo, masterIndex);
            if(ctx.getNetworkConfig().getEncryptControlMessages()) {
                received = pkt.verifyAndDecrypt(gcm);
            } else {
                received = pkt.verify(gcm);
            }
            if(ENABLE_CRYPTO_DOWNLINK_DBG)
                if(!received) print_dbg("[SD] verify failed!\n");
        }
    }
#endif // ifdef CRYPTO
    return received;
}

void DynamicScheduleDownlinkPhase::applyInfoElements(SchedulePacket& spkt)
{
    std::vector<ScheduleElement> elements = spkt.getElements();
    std::vector<InfoElement> infos;
    // Check for Info Elements and separate them from Schedule Elements
    auto firstInfo = std::find_if(elements.begin(), elements.end(),
                                  [](ScheduleElement s){
                                      return (s.getTx()==0 && s.getRx()==0);
                                  });
    if(firstInfo != elements.end())
    {
        infos = std::vector<InfoElement>(firstInfo, elements.end());
        spkt.popElements(infos.size());
    }

    if(!infos.empty()) streamMgr->applyInfoElements(infos);
}

void DynamicScheduleDownlinkPhase::initSchedule(SchedulePacket& spkt)
{
    applyInfoElements(spkt); //Must be done first as it removes them
    
    if(ENABLE_SCHEDULE_DIST_DYN_INFO_DBG)
        print_dbg("[SD] Node:%d New schedule received!\n", myId);

    // Save last schedule ID
    lastScheduleID = header.getScheduleID();
    // Replace old schedule header and elements
    header = spkt.getHeader();
    schedule = spkt.getElements();
    // Resize the received bool vector to the size of the new schedule
    received.clear();
    received.resize(header.getTotalPacket(), 0);
    // Set current packet as received
    received.at(header.getCurrentPacket()) = 1;

    /**
     * Initialize sendingRounds counter info.
     * If the first schedule packet received is not the first that was sent,
     * we have to count the number of packets that were lost initially and
     * skip ahead in the counter.
     */
    sendingRounds = header.getTotalPacket() * scheduleRepetitions;
    currentSendingRound = 1 + header.getRepetition() * scheduleRepetitions
                            + header.getCurrentPacket();
}

void DynamicScheduleDownlinkPhase::appendToSchedule(SchedulePacket& spkt, bool beginResend)
{
    applyInfoElements(spkt); //Must be done first as it removes them
    
    auto newHeader = spkt.getHeader();
    
    if(ENABLE_SCHEDULE_DIST_DYN_INFO_DBG)
        print_dbg("[SD] Node:%d Piece %d of schedule received!\n",
                  myId, newHeader.getCurrentPacket());
    
    if(header.getTotalPacket()   != newHeader.getTotalPacket()   ||
       header.getScheduleID()    != newHeader.getScheduleID()    ||
       header.getScheduleTiles() != newHeader.getScheduleTiles() ||
       (beginResend==false && header.getActivationTile() != newHeader.getActivationTile()))
    {
        if(ENABLE_SCHEDULE_DIST_DBG)
            print_dbg("[SD] BUG: appendToSchedule header differs, refusing to apply\n");
        return; //Not applying this schedule packet
    }
    header = newHeader;

    // Add elements from received packet to new schedule only if this is the
    // first time these elements are being received
    if(received.at(newHeader.getCurrentPacket()) == 0) {
        std::vector<ScheduleElement> elements = spkt.getElements();
        schedule.insert(schedule.end(), elements.begin(), elements.end());
    }
    // Set current packet as received
    received.at(newHeader.getCurrentPacket())++;
}

bool DynamicScheduleDownlinkPhase::isScheduleComplete()
{
    // If no packet was received, the schedule is not complete
    if(received.size() == 0) return false;
    for(auto pkt : received) if(pkt==0) return false;
    return true;
}

void DynamicScheduleDownlinkPhase::resetAndDisableSchedule(long long slotStart)
{
    if(ENABLE_SCHEDULE_DIST_DBG)
        print_dbg("[SD] incomplete schedule %s\n",scheduleStatusAsString().c_str());
    
    header = ScheduleHeader();
    schedule.clear();
    received.clear();
    incompleteScheduleCounter = 0;

    auto currentTile = ctx.getCurrentTile(slotStart);
    dataPhase->applySchedule(std::vector<ExplicitScheduleElement>(),
                             std::map<StreamId, std::pair<unsigned char, unsigned char>>(),
                             header.getScheduleID(),
                             header.getScheduleTiles(),
                             header.getActivationTile(), currentTile);
    //NOTE: applying an empty schedule to the Stream Manager closes all the streams,
    //causing applications to send another CONNECT when a new schedule is
    //finally received, resulting in the distribution of two schedules instead
    //of one.
    //streamMgr->applySchedule(schedule);
    ctx.getStreamManager()->enqueueSME(StreamManagementElement::makeResendSME(myId));
}

void DynamicScheduleDownlinkPhase::handleIncompleteSchedule()
{
    const int timeout = ctx.getNetworkConfig().getMaxNodes()*2; //Trying a reasonable timeout
    if(++incompleteScheduleCounter >= timeout)
    {
        incompleteScheduleCounter = 0;
        ctx.getStreamManager()->enqueueSME(StreamManagementElement::makeResendSME(myId));
    }
}

std::string DynamicScheduleDownlinkPhase::scheduleStatusAsString() const
{
    std::string result;
    result.reserve(received.size() + 2);
    result += '[';
    for(auto pkt : received) result += ('0' + pkt); //Assuming pkt<10
    result += ']';
    return result;
}

void DynamicScheduleDownlinkPhase::printHeader(ScheduleHeader& header) const
{
    print_dbg("[SD] node %d, hop %d, received schedule %u/%u/%lu/%d\n",
              ctx.getNetworkId(),
              ctx.getHop(),
              header.getTotalPacket(),
              header.getCurrentPacket(),
              header.getScheduleID(),
              header.getRepetition());
}

}
