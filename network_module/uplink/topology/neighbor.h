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

#pragma once

namespace mxnet {

class Neighbor {
public:
    Neighbor()  = delete;
    Neighbor(unsigned char nodeId, unsigned char unseenSince = 0) : nodeId(nodeId), unseenSince(unseenSince) {}
    virtual ~Neighbor() {};
    unsigned char getNodeId() const { return nodeId; }
    void seen() { unseenSince = 0; }
    void unseen() { unseenSince++; }
    unsigned char getUnseen() const { return unseenSince; }
    bool operator <(const Neighbor &b) const { return nodeId < b.nodeId; };
    bool operator >(const Neighbor &b) const { return b < *this; }
    bool operator ==(const Neighbor &b) const { return nodeId == b.nodeId; }
    bool operator !=(const Neighbor &b) const { return nodeId != b.nodeId; }
    bool operator <(const unsigned char &b) const { return nodeId < b; };
    bool operator >(const unsigned char &b) const { return nodeId > b; }
    bool operator ==(const unsigned char &b) const { return nodeId == b; }
    bool operator !=(const unsigned char &b) const { return nodeId != b; }
protected:
    unsigned char nodeId;
    unsigned char unseenSince;
};

class Predecessor : public Neighbor {
public:
    Predecessor(unsigned char nodeId, short rssi, unsigned char unseenSince = 0) :
        Neighbor(nodeId, unseenSince), rssi(rssi) {};
    Predecessor() = delete;
    short getRssi() const { return rssi; }
    void setRssi(short rssi) { this->rssi = rssi; }

    struct CompareRSSI {
        bool operator()(const Predecessor& left, const Predecessor& right) const {
            return left.getUnseen() < right.getUnseen();
        }
    };
protected:
    short rssi;
};

class Successor : public Neighbor {
public:
    Successor() = delete;
    Successor(unsigned char nodeId, unsigned char unseenSince = 0) : Neighbor(nodeId, unseenSince) {};
};
} /* namespace mxnet */

