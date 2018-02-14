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

#ifndef DEBUG_SETTINGS_H
#define DEBUG_SETTINGS_H

//prints info if receiving a packet
const bool ENABLE_PKT_INFO_DBG = false;

//dumps the contents of the packets, ENABLE_BAD_PKT_INFO must be defined
const bool ENABLE_PKT_DUMP_DBG = false;

//prints the exception if any while using the radio
const bool ENABLE_RADIO_EXCEPTION_DBG = true;

//prints the flooding phase debug info
const bool ENABLE_FLOODING_INFO_DBG = true;

//prints the flooding phase errors
const bool ENABLE_FLOODING_ERROR_DBG = true;

//prints the flooding phase debug info
const bool ENABLE_ROUNDTRIP_INFO_DBG = true;

//prints the flooding phase errors
const bool ENABLE_ROUNDTRIP_ERROR_DBG = true;

#endif /* DEBUG_SETTINGS_H */

