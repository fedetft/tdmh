/***************************************************************************
 *   Copyright (C) 2012, 2013, 2014, 2015, 2016 by Terraneo Federico and   *
 *      Luigi Rinaldi                                                      *
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

#include "transceiver.h"

#include "RadioMessage.h"
#include "omnetpp.h"
#include <cstring>
#include <algorithm>

using namespace std;

namespace miosix {
std::mutex Transceiver::instanceMutex;
std::map<NodeBase*, Transceiver*> Transceiver::instances;

const bool Transceiver::SIM_DBG = false;

Transceiver::Transceiver() : MiosixInterface() {
}

Transceiver& Transceiver::instance() {
    auto* curNode = MiosixStaticInterface::getNode();
    Transceiver* retval;
    std::map<NodeBase*, Transceiver*>::iterator it = Transceiver::instances.find(curNode);
    if (it == Transceiver::instances.end()) {
        std::lock_guard<std::mutex> myLock(Transceiver::instanceMutex);
        it = Transceiver::instances.find(curNode);
        if (it == Transceiver::instances.end()) {
            retval = new Transceiver();
            Transceiver::instances[curNode] = retval;
        }
    } else retval = it->second;
    return *retval;
}

void Transceiver::deinstance(NodeBase* ref) {
    std::map<NodeBase*, Transceiver*>::iterator it = Transceiver::instances.find(ref);
    if (it != Transceiver::instances.end()) {
        delete it->second;
        Transceiver::instances.erase(it);
    }
}

Transceiver::~Transceiver() {
}

void Transceiver::configure(const TransceiverConfiguration& config) {
    if(config.frequency < minFrequency || config.frequency > maxFrequency)
        throw range_error("config.frequency");
    cfg = config;
}

void Transceiver::sendNow(const void* pkt, int size, string pktName) {
    sendAt(pkt, size, simTime().inUnit(SIMTIME_NS), pktName, Unit::NS);
}

bool Transceiver::sendCca(const void* pkt, int size) {
    throw runtime_error("Unimplementable");
}

void Transceiver::sendAt(const void* pkt, int size, long long when, string pktName, Unit unit) {
    if(!isOn) throw runtime_error("Cannot send with transceiver turned off!");
    if (unit != Unit::NS) throw runtime_error("Not implemented");
    auto waitTime = SimTime(when, SIMTIME_NS) - simTime();
    if(waitTime < 0)
        throw runtime_error("Transceiver::sendAt too late to send");
    parentNode->waitAndDeletePackets(waitTime);

    unsigned char finalPkt[size + (cfg.crc? 2 : 0)];
    memcpy(finalPkt, pkt, size);
    if (cfg.crc) {
        if (size > RadioMessage::dataSize - 2)
            throw runtime_error(string("Packet too long ")+to_string(size));
        auto crc = computeCrc(pkt, size);
        finalPkt[size] = crc & 0xFF;
        finalPkt[size + 1] = crc >> 8;
    }
    auto actualSize = size + (cfg.crc? 2 : 0);
    if (SIM_DBG)
        EV_INFO << "Sent packet of length " << actualSize << " at " << simTime().inUnit(SIMTIME_NS) <<
        " finishing at " << simTime().inUnit(SIMTIME_NS) + RadioMessage::getPPDUDuration(actualSize) << endl;
    for(int i=0; i<parentNode->gateSize("wireless"); i++)
        parentNode->send(new RadioMessage(finalPkt, actualSize, pktName), "wireless$o", i);

    parentNode->waitAndDeletePackets(SimTime(RadioMessage::getPPDUDuration(actualSize), SIMTIME_NS));
}

RecvResult Transceiver::recv(void* pkt, int size, long long timeout, Unit unit, Correct c) {
    if(!isOn) throw runtime_error("Cannot receive with transceiver turned off!");
    if (unit != Unit::NS) throw runtime_error("Not implemented");
    RecvResult result;
    cMessage* msg;
    if (timeout == infiniteTimeout) {
        msg = parentNode->receive();
    } else {
        auto waitDelta = SimTime(timeout, SIMTIME_NS) - simTime();
        msg = parentNode->receive(waitDelta);
    }
    auto* cPkt = dynamic_cast<RadioMessage*>(msg);
    if (msg == nullptr || !cPkt) {
        result.error = RecvResult::TIMEOUT;
        delete msg;
        return result; //no message received
    }
    result.timestamp = msg->getSendingTime().inUnit(SIMTIME_NS);
    result.timestampValid = true;
    result.rssi = 5;
    auto packetDuration = RadioMessage::getPPDUDuration(cPkt->length);
    if ((cfg.strictTimeout? packetDuration : RadioMessage::preambleSfdTimeNs) + result.timestamp > timeout) {
        if (SIM_DBG)
            EV_INFO << "Packet sent at " << result.timestamp << " and finished receiving at " <<
            (packetDuration + result.timestamp) << " timed out " << (cfg.strictTimeout? "strictly ": "") <<
            "at " << timeout << endl;
        result.error = RecvResult::TIMEOUT;
        return result;//packet received but exceeds the timeout
    }
    cQueue interferingMsgs, collidingMsgs;
    //wait for the max confidence time to obtain a constructive interference
    parentNode->waitAndEnqueue(SimTime(RadioMessage::constructiveInterferenceTimeNs, SIMTIME_NS), &interferingMsgs);
    //and wait for the whole message length
    auto msgDeadlineDelta = packetDuration - RadioMessage::constructiveInterferenceTimeNs;
    parentNode->waitAndEnqueue(SimTime(msgDeadlineDelta, SIMTIME_NS), &collidingMsgs);
    if (!collidingMsgs.isEmpty()) {
        if (SIM_DBG)
            EV_INFO << "Packet sent at " << result.timestamp << " and finished receiving at " <<
            (packetDuration + result.timestamp) << " collided with at least another sent at " <<
            dynamic_cast<cMessage*>(collidingMsgs.back())->getSendingTime().inUnit(SIMTIME_NS) <<
            ", crc was " << (cfg.crc? "enabled": "disabled") << endl;
        //if there was a collision, meaning any received packet during the message length
        if (cfg.crc) //crc enabled -> bad crc
            result.error = RecvResult::CRC_FAIL;
        else { //crc disabled -> random bytes received successfully
            for (int i = 0; i < size; i++)
                ((unsigned char*) pkt)[i] = parentNode->uniform(0, 16, 0);
            result.error = RecvResult::OK;
        }
        delete msg;
        while(!collidingMsgs.isEmpty()) delete collidingMsgs.pop();
        while(!interferingMsgs.isEmpty()) delete interferingMsgs.pop();
        return result; //message collided and arrived corrupted
    }
    result.error = RecvResult::OK;
    result.size = cPkt->length;
    unsigned char* correlated;
    if (!interferingMsgs.isEmpty()) {
        if (SIM_DBG)
            EV_INFO << "Packet sent at " << result.timestamp << " and finished receiving at " <<
            (packetDuration + result.timestamp) << " constructively interfered with other " <<
            interferingMsgs.getLength() << " packets" << endl;
        //in case of interfering messages correlate them with their
        //maximum length and chosing a random byte among those received.
        std::vector<RadioMessage*> packets;
        std::list<int> lengths;
        packets.push_back(cPkt);
        lengths.push_back(result.size);
        for (cQueue::Iterator it(interferingMsgs); !it.end(); it++) {
            auto* tmp = dynamic_cast<RadioMessage*>(*it);
            if (tmp) {
                packets.push_back(tmp);
                if (tmp->length > result.size) result.size = tmp->length;
                lengths.push_back(tmp->length);
            }
        }
        correlated = new unsigned char[result.size];
        //warning: supposing that if interfering, no timing offset in packet symbols is involved
        // and also it is avoided to correlate the bytes by their PN sequence.
        //Only the theoretical results of Glossy are taken into account.
        for(int i = 0; i < result.size; i++) {
            auto numPkts = std::count_if(lengths.begin(), lengths.end(), [i](int e){return e > i;});
            auto chosenPkt = parentNode->uniform(0, numPkts, 0);
            auto actualPkt = -1;
            int j = 0;
            for (auto len = lengths.begin(); j <= chosenPkt; len++) {
                if (*len > i) j++;
                actualPkt++;
            }
            correlated[i] = packets[actualPkt]->data[i];
        }
        while(!interferingMsgs.isEmpty()) delete interferingMsgs.pop();
    } else {
        correlated = new unsigned char[result.size];
        memcpy(correlated, cPkt->data, result.size);
        if (SIM_DBG)
            EV_INFO << "Packet sent at " << result.timestamp << " and finished receiving at " <<
            (packetDuration + result.timestamp) << " successfully" << endl;
    }
    delete msg;
    if (cfg.crc) {
        result.size -= 2;
        auto crc = computeCrc(correlated, result.size);
        if (( correlated[result.size] | (correlated[result.size + 1] << 8)) != crc)
            result.error = RecvResult::CRC_FAIL;
    }
    if (result.size > size) {
        result.error = RecvResult::TOO_LONG;
    }
    memcpy(pkt, correlated, std::min<int>(size, result.size));
    delete[] correlated;
    if (size > result.size) memset(((unsigned char*) pkt) + result.size, 0, size - result.size);
    return result;
}

uint16_t Transceiver::computeCrc(const void* data, int size) {
    crcMutex.lock();
    crc.reset();
    crc.process_bytes(data, size);
    auto retval = static_cast<uint16_t>(crc.checksum());
    crcMutex.unlock();
    return retval;
}

void TransceiverConfiguration::setChannel(int channel)
{
    if(channel<11 || channel>26) throw std::range_error("Channel not in range");
    frequency=2405+5*(channel-11);
}
}
