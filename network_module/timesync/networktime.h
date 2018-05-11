/***************************************************************************
 *   Copyright (C)  2018 by Terraneo Federico, Polidori Paolo              *
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

#include <miosix.h>

namespace mxnet {

/**
 * A time that is unique throughout the network.
 * At least as long as clock synchronization is running and sync packets are
 * regularly received, this time should match that of the master node +/- the
 * clock synchronization error that is within one microsecond unless a
 * temperature chenge is occurring
 */
class NetworkTime
{
public:
    /**
     * Default constructor, initializes time to zero
     */
    NetworkTime() : time(0) {}
    
    /**
     * Constructor from long long, explicit to prevent unwanted casts between
     * other times in the node and NetworkTime
     */
    explicit NetworkTime(long long time) : time(time) {}
    
    /**
     * Explicit way to cast a NetworkTime to a long long
     * \return the network time as long long
     */
    inline long long get() const { return time; }
    
    /**
     * Equivalent of miosix::getTime() that returns the global network time.
     * \return The current network time
     */
    static inline NetworkTime getTime()
    {
        return NetworkTime(miosix::getTime()+localNodeToNetworkTimeOffset);
    }
    
    /**
     * \internal
     * Set the offset between the local node time (miosix::getTime()) and
     * the network time, which is the time of the master node in the network.
     *
     * Note that the local node time is already corrected in skew to match the
     * master node rate as long as clock synchronization packets are received.
     * This is done at the OS level, but the OS time cannot have clock jumps
     * as it would break sleeping tasks, hence only the network time is
     * corrected for offset when joining a network.
     */
    static void setLocalNodeToNetworkTimeOffset(long long offset)
    {
        localNodeToNetworkTimeOffset=offset;
    }
    
private:
    long long time; ///< Network time
    static long long localNodeToNetworkTimeOffset; ///< Offset
};

inline NetworkTime operator+ (NetworkTime lhs, NetworkTime rhs)
{
    return NetworkTime(lhs.get() + rhs.get());
}

inline NetworkTime operator- (NetworkTime lhs, NetworkTime rhs)
{
    return NetworkTime(lhs.get() - rhs.get());
}

inline void operator+= (NetworkTime& lhs, NetworkTime rhs)
{
    lhs += rhs;
}

inline void operator-= (NetworkTime& lhs, NetworkTime rhs)
{
    lhs -= rhs;
}

//NOTE: no operator * and / on purpose as 64x64 mult/div are slow ans should be avoided

} // namespace mxnet
