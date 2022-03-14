/***************************************************************************
 *   Copyright (C) 2022 by Luca Conterio                                   *
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
#include <network_module/tdmh.h>
#include <experiments/control_demo/src/utils.h>
#include <experiments/control_demo/src/temp_sensor.h>
#include <thread>

using namespace std;
using namespace mxnet;
using namespace miosix;

class SensorNode {

public:
    SensorNode(MACContext* c, TempSerialSensor* s, unsigned char p = 1) : ctx(c), sensor(s), port(p) {}

    void run() {
        while(!ctx->isReady()) {
            Thread::sleep(1000);
        }
        /* Delay the Stream opening so it gets opened after the StreamServer */
        Thread::sleep(streamOpeningDelay);

        /* Used to have higher priority than main */
        Thread::create(&SensorNode::streamThreadLauncher, 2048, MAIN_PRIORITY+1, this);

        while(true);
    }

private:
    const unsigned int streamOpeningDelay = 1000;
    MACContext *ctx;
    TempSerialSensor* sensor;
    const unsigned char port = 0;
    unsigned int counter = 0;

    int openStream(unsigned char dest) {
        try {
            printf("[A] N=%d Waiting to authenticate master node\n", ctx->getNetworkId());
            while(waitForMasterTrusted()) {
                //printf("[A] N=%d StreamManager not present! \n", ctx->getNetworkId());
            }

            /* Open a Stream to another node */
            printf("[A] Opening stream to node %d\n", dest);
            int stream = connect(dest,         // Destination node
                                port,          // Destination port
                                clientParams,  // Stream parameters 
                                2*ctx->getDataSlotDuration()); // Wakeup advance (max 1 tile)

            return stream;

        } catch(exception& e) {
            printf("exception %s\n",e.what());
            return -1;
        }
    }

    void streamThread() {
        while(true) {
            int stream = openStream(controllerNodeID);
            if(stream < 0) {                
                printf("[A] Stream opening failed! error=%d\n", stream);
                Thread::sleep(5000);
                continue;
            }

            bool first = true;
            try {
                while(getInfo(stream).getStatus() == StreamStatus::ESTABLISHED) {
                    if(first) {
                        first=false;
                        printf("[A] Stream opened \n");
                    }

                    /*int res =*/ mxnet::wait(stream);
                    // if (res != 0) {
                    //     printf("[A] Stream wait error\n");
                    // }

                    int lastSensorNodeSample = sensor->getLastSample();
                    counter++;
                    Data data(sensorNodeID, counter, lastSensorNodeSample);

                    /*int ret =*/ mxnet::write(stream, &data, sizeof(data));      
                    // if(ret >= 0) {
                    //     printf("[A] Sent ID=%d NT=%lld C=%u\n",
                    //             data.getId(), data.getNetworkTime(), data.getCounter());
                    // }
                    // else
                    //     printf("[E] Error sending data, result=%d\n", ret);
                }

                printf("[A] Stream (%d,%d) closed, status=", ctx->getNetworkId(), controllerNodeID);
                printStatus(getInfo(stream).getStatus());
                // NOTE: Remember to call close() after the stream has been closed
                mxnet::close(stream);
            } catch(exception& e) {
                printf("exception %s\n",e.what());
            }
        }
    }

    static void streamThreadLauncher(void* arg) {
        reinterpret_cast<SensorNode*>(arg)->streamThread();
    }
};