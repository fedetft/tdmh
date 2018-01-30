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

#ifndef NETWORKPHASE_H
#define NETWORKPHASE_H

#include "mediumaccesscontroller.h"
#include <utility>

namespace miosix {
    class MACContext;
    class MACPhase {
    public:
        MACPhase() = delete;
        /**
         * Constructor to be used if the node is the first one sending in this phase
         * @param startTime
         * @param activityTime
         */
        MACPhase(long long startTime, long long activityTime) :
                MACPhase(startTime, activityTime, activityTime) {};
        /**
         * Constructor to be used if the node has to wait before acting in this phase
         * @param startTime
         * @param globalActivityTime
         * @param activityTime
         */
        MACPhase(long long startTime, long long globalActivityTime, long long activityTime) :
                globalStartTime(startTime),
                globalFirstActivityTime(globalActivityTime),
                localFirstActivityTime(activityTime) {};
        MACPhase(const MACPhase& orig) = delete;
        virtual ~MACPhase();
        virtual void execute(MACContext& ctx) = 0;
    protected:
        
        /**
         * Represents when the network theoretically switched to this phase.
         * Can be considered a worst case start time of execution.
         */
        const long long globalStartTime;
        /**
         * Represents when the node making the first action of the phase must do it.
         */
        const long long globalFirstActivityTime;
        /**
         * Represents when at least one node in the network starts this phase.
         */
        const long long localFirstActivityTime;
    private:

    };
}

#include "maccontext.h"

#endif /* NETWORKPHASE_H */

