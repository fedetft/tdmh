/***************************************************************************
 *   Copyright (C) 2017 by Polidori Paolo, Terraneo Federico               *
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

#include <cstdio>
#include <miosix.h>
#include "network_module/macround/mastermacround.h"
#include "network_module/mediumaccesscontroller.h"

using namespace std;
using namespace miosix;

void flopsyncRadio(void*){    
    printf("Dynamic node\n");
    //MediumAccessController& controller = MediumAccessController::instance(new DynamicMACRound::DynamicMACRoundFactory(), 6, 1, 2450);
    MediumAccessController& controller = MediumAccessController::instance(new MasterMACRound::MasterMACRoundFactory(), 6, 1, 2450);
    controller.run();
}

int main()
{
    auto t1 = Thread::create(flopsyncRadio,2048,PRIORITY_MAX-1, nullptr, Thread::JOINABLE);
    
    t1->join();
    printf("Dying now...\n");
    
    return 0;
}
