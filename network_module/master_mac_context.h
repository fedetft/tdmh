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

#include "mac_context.h"
#include "timesync/master_timesync_downlink.h"
#include "uplink/topology/mesh_topology_context.h"
#include "uplink/topology/tree_topology_context.h"
#include "uplink/master_uplink_phase.h"
#include "uplink/stream_management/stream_management_context.h"
#include "schedule/master_schedule_downlink.h"

namespace mxnet {

class MasterMACContext : public MACContext {
public:
    MasterMACContext(const MediumAccessController& mac, miosix::Transceiver& transceiver, const NetworkConfiguration& config);
    MasterMACContext() = delete;
    virtual ~MasterMACContext() {};
protected:
    long long initializationDelay = 1000000;
};

} /* namespace mxnet */
