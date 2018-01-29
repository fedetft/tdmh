/***************************************************************************
 *   Copyright (C)  2017 by Terraneo Federico, Polidori Paolo              *
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

#ifndef MASTERMACROUND_H
#define MASTERMACROUND_H

#include "macround.h"
#include "macroundfactory.h"
#include "../flooding/masterfloodingphase.h"
#include "../roundtrip/listeningroundtripphase.h"
#include "../reservation/masterreservationphase.h"
#include "../assignment/masterassignmentphase.h"

namespace miosix {
    class MasterMACRound : public MACRound {
    public:
        MasterMACRound() = delete;
        explicit MasterMACRound(const MediumAccessController& mac) :
                MasterMACRound(mac, getTime() + initializationDelay) {}
        MasterMACRound(const MasterMACRound& orig) = delete;
        virtual ~MasterMACRound();
        virtual void run(MACContext& ctx) override;

        class MasterMACRoundFactory : public MACRoundFactory {
        public:
            MasterMACRoundFactory() {};
            MACRound* create(MACContext& ctx) const override;
            virtual ~MasterMACRoundFactory() {};
        };
        
    protected:
        MasterMACRound(const MediumAccessController& mac, long long roundStart) :
                MACRound(
                    new MasterFloodingPhase(roundStart),
                    new ListeningRoundtripPhase(flooding->getPhaseEnd()),
                    new MasterReservationPhase(roundtrip->getPhaseEnd()),
                    new MasterAssignmentPhase(reservation->getPhaseEnd())),
                roundStart(roundStart) {}
                
        /**
         * Initial skew for allowing the master to boot the network module before starting the first network round
         */
        long long initializationDelay = 200000;

    private:
        long long roundStart;
    };
}

#endif /* MASTERMACROUND_H */

