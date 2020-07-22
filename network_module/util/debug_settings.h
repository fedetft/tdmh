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

namespace mxnet {

//less understandable but short messages, useful when bandwidth is at limit
const bool COMPRESSED_DBG = true;
    
//prints info if receiving a packet
const bool ENABLE_PKT_INFO_DBG = false;

//dumps the contents of the packets, ENABLE_BAD_PKT_INFO must be defined
const bool ENABLE_PKT_DUMP_DBG = false;

//prints the exception if any while using the radio
const bool ENABLE_RADIO_EXCEPTION_DBG = true;

//prints the timesync downlink phase debug info
const bool ENABLE_TIMESYNC_DL_INFO_DBG = true;

//prints the timesync downlink phase errors
const bool ENABLE_TIMESYNC_ERROR_DBG = true;

//prints the roundtrip subphase debug info
const bool ENABLE_ROUNDTRIP_INFO_DBG = false;

//prints the roundtrip subphase errors
const bool ENABLE_ROUNDTRIP_ERROR_DBG = false;

//prints lightweight uplink message info
const bool ENABLE_UPLINK_DBG = true;

//prints the uplink phase debug verbose info
const bool ENABLE_UPLINK_VERB_DBG = false;

//print every bitmask received by the master
const bool ENABLE_TOPOLOGY_BITMASK_DBG = true;

//prints the topology map at each uplink phase
const bool ENABLE_TOPOLOGY_INFO_DBG = false;

//prints the slot number, if packets received and RSSI
const bool ENABLE_TOPOLOGY_SHORT_SUMMARY = false;

//prints the uplink phase debug info
const bool ENABLE_UPLINK_DYN_INFO_DBG = true;

//prints the uplink phase debug verbose info
const bool ENABLE_UPLINK_DYN_VERB_DBG = true;

//prints the topology map at each uplink phase
const bool ENABLE_TOPOLOGY_DYN_INFO_DBG = false;

//prints the slot number, if packets received and RSSI
const bool ENABLE_TOPOLOGY_DYN_SHORT_SUMMARY = false;

//prints the stream context debug info
const bool ENABLE_STREAM_INFO_DBG = true;

//prints summary information on scheduling in the master node
const bool SCHEDULER_SUMMARY_DBG = true;

//prints detailed information on scheduling in the master node
const bool SCHEDULER_DETAILED_DBG = false;

//lightweight prints for schedule distribution
const bool ENABLE_SCHEDULE_DIST_DBG = true;

//prints the schedule distribution info in the master node
const bool ENABLE_SCHEDULE_DIST_MAS_INFO_DBG = false;

//prints the schedule distribution info in the dynamic nodes
const bool ENABLE_SCHEDULE_DIST_DYN_INFO_DBG = false;

//prints the data phase debug info
const bool ENABLE_DATA_INFO_DBG = true;

//prints the data phase errors
const bool ENABLE_DATA_ERROR_DBG = true;

//prints the Stream Manager info messages
const bool ENABLE_STREAM_MGR_INFO_DBG = true;

//prints debug messages for state changes in KeyManager
const bool ENABLE_CRYPTO_KEY_MGMT_DBG = true;

//prints debug information about rekeying
const bool ENABLE_CRYPTO_REKEYING_DBG = true;

//prints packet synchronization numbers in dataphase
const bool ENABLE_CRYPTO_DATA_DBG = true;

//prints packet synchronization numbers and verify errors in timesync
const bool ENABLE_CRYPTO_TIMESYNC_DBG = true;

//prints packet synchronization numbers and verify errors in schedule distribution
const bool ENABLE_CRYPTO_DOWNLINK_DBG = true;

//prints packet synchronization numbers and verify errors in uplink
//NOTE: this is too verbose. Use only to debug nonces.
const bool ENABLE_CRYPTO_UPLINK_DBG = false;


#ifdef _MIOSIX
#define DEBUG_MESSAGES_IN_SEPARATE_THREAD
#endif

/**
 * If you want to override this function's behavior, define the macro
 * #define print_dbg myfun
 * and define the function myfun.
 * Do this anywhere before including any network_module file.
 */
void print_dbg(const char *fmt, ...);

/**
 * Throw logic error with format string, limited to 128 characters
 */
void throwLogicError(const char *fmt, ...);

/**
 * Throw runtime error with format string, limited to 128 characters
 */
void throwRuntimeError(const char *fmt, ...);

}


