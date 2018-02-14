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


#ifndef TRANSCEIVERCONFIGURATION_H_
#define TRANSCEIVERCONFIGURATION_H_

class TransceiverConfiguration
{
public:
    TransceiverConfiguration(int frequency=2450, int txPower=0, bool crc=true,
                             bool strictTimeout=true)
        : frequency(frequency), txPower(txPower), crc(crc),
          strictTimeout(strictTimeout) {}

    /**
     * Configure the frequency field of this class from a IEEE 802.15.4
     * channel number
     * \param channel IEEE 802.15.4 channel number (from 11 to 26)
     */
    void setChannel(int channel);

    int frequency;      ///< TX/RX frequency, between 2394 and 2507
    int txPower;        ///< TX power in dBm
    bool crc;           ///< True to add CRC during TX and check it during RX
    bool strictTimeout; ///< Used only when receiving. If false and an SFD has
                        ///< been received, prolong the timeout to receive the
                        ///< packet. If true, return upon timeout even if a
                        ///< packet is being received
};

#endif /* TRANSCEIVERCONFIGURATION_H_ */
