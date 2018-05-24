/***************************************************************************
 *   Copyright (C)  2013,2018 by Terraneo Federico                         *
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

#ifndef FLOPSYNC_2_H
#define	FLOPSYNC_2_H

#include "../controller/synchronizer.h"

/**
 * A new flopsync controller that can reach zero steady-state error both
 * with step-like and ramp-like disturbances.
 * It provides better synchronization under temperature changes, that occur
 * so slowly with respect to the controller operation to look like ramp
 * changes in clock skew.
 */
class Flopsync2 : public Synchronizer
{
public:
    /**
     * Constructor
     */
    Flopsync2() { reset(); }
    
    /**
     * Compute clock correction and receiver window given synchronization error
     * \param error synchronization error
     * \return a pair with the clock correction, and the receiver window
     */
    std::pair<int,int> computeCorrection(int error);
    
    /**
     * Compute clock correction and receiver window when a packet is lost
     * \return a pair with the clock correction, and the receiver window
     */
    std::pair<int,int> lostPacket();
    
    /**
     * Used after a resynchronization to reset the controller state
     */
    void reset();
    
    /**
     * \return the synchronization error e(k)
     */
    int getSyncError() const { return eo; }
    
    /**
     * \return the clock correction u(k)
     */
    int getClockCorrection() const;
    
    /**
     * \return the receiver window (w)
     */
    int getReceiverWindow() const { return dw; }
    
private:
    int eo, eoo;
    int uo, uoo;
    int sum;
    int squareSum;
    int threeSigma;
    int dw;
    unsigned char count;
    char init;
    
    static const int wMin=  50000; //50us
    static const int wMax=6000000; //6ms
    
    static const int numSamples=5; //Number of samples for variance compuation
    static const int controllerScaleFactor=6;
    static const int varianceScaleFactor=300;
};

#endif //FLOPSYNC_2_H

