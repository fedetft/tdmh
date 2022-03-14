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
#include <experiments/control_demo/src/temp_controller.h>

using namespace std;
using namespace mxnet;
using namespace miosix;

class ControllerNode {

public:
    ControllerNode(MACContext* c) : ctx(c) {}

    void run() {
        while(!ctx->isReady()) {
            printf("waiting...\n");
            Thread::sleep(1000);
        }

        // thread that accepts incoming connections
        while(true) {
            int server = openServer(PORT1);
            if(server < 0) {                
                printf("[A] Server opening failed! error=%d\n", server);
                Thread::sleep(5000);
                continue;
            }

            try {
                while(getInfo(server).getStatus() == StreamStatus::LISTEN) {
                    int stream = accept(server);
                    StreamInfo info = getInfo(stream);
                    if (info.getSrc() == sensorNodeID) {
                        Thread::create(&ControllerNode::processingSensorThreadLauncher, 2048, 
                                            MAIN_PRIORITY+1, new StreamThreadPar(this, stream));
                    }
                    else {
                        Thread::create(&ControllerNode::processingActuatorThreadLauncher, 2048, 
                                            MAIN_PRIORITY+1, new StreamThreadPar(this, stream));
                    }
                }
            } catch(exception& e) {
                printf("Unexpected exception while server accept: %s\n",e.what());
            } catch(...) {
                printf("Unexpected unknown exception while server accept\n");
            }
            printf("[A] Server on port %d closed, status=", PORT1);
            printStatus(getInfo(server).getStatus());
            mxnet::close(server);
        }
    }

    void processSensor(StreamThreadPar* arg) {
        auto *s = reinterpret_cast<StreamThreadPar*>(arg);
        int inStream = s->stream;
        StreamInfo info = getInfo(inStream);
        StreamId id = info.getStreamId();
        print_dbg("[A] Stream (%d,%d) accepted\n", id.src, id.dst);

        if (!outStreamOpened) {
            Thread::sleep(streamOpeningDelay);
            openControllerOutputStream();
        }
        
        try {
            while(getInfo(inStream).getStatus() == StreamStatus::ESTABLISHED) {

                Data data;
                int len = mxnet::read(inStream, &data, sizeof(data));
                long long now = mxnet::NetworkTime::now().get();

                if(len > 0) {
                    if(len == sizeof(data))
                    {
                        if (outStreamOpened) {
                            int controlAction = runController(data.getValue());
                            send(outStream, controlAction, data.getSampleTime(), data.getCounter());
                        }
                        
                        printf("[A] R (%d,%d) ID=%d NT=%lld C=%u V=%d\n",
                        id.src, id.dst, data.getId(), data.getNetworkTime(), data.getCounter(), data.getValue());
                        long long latency = now - data.getNetworkTime();
                        printf("[A] (%d,%d): L=%lld\n", id.src, id.dst, latency);
                    } else {
                        printf("[E] W (%d,%d) %d\n", id.src, id.dst, len);
                    }
                }
                else if(len == -1) {
                    tempController.skipStep();
                    print_dbg("[E] M (%d,%d)\n", id.src, id.dst);
                }
                else {
                    print_dbg("[E] M (%d,%d) Read returned %d\n", id.src, id.dst, len);
                }

                //printStatus(getInfo(outStream).getStatus());
                if (!outStreamOpened) { // first iteration, stream not opened yet
                    Thread::sleep(streamOpeningDelay);
                    openControllerOutputStream();
                }
                // since data to actuator are sent as a reaction for data received from sensor,
                // in sensor data handling check output stream status
                // if the stream was opened and it is now not established, close and reopen
                else if (outStreamOpened && getInfo(outStream).getStatus() != StreamStatus::ESTABLISHED) {  
                    printf("[A] Closing output stream\n");
                    closeStream(outStream);
                    outStreamOpened = false;
                    outStream = -1;
                }
            }

            delete s;
            closeStream(inStream);
            // if (outStreamOpened && outStream == -1) {
            //     closeStream(outStream);
            // }
        } catch(exception& e) {
            printf("exception %s\n",e.what());
        }
    }

    void processActuator(StreamThreadPar* arg) {
        auto *s = reinterpret_cast<StreamThreadPar*>(arg);
        int inStream = s->stream;
        StreamInfo info = getInfo(inStream);
        StreamId id = info.getStreamId();
        print_dbg("[A] Stream (%d,%d) accepted\n", id.src, id.dst);
        
        try {
            while(getInfo(inStream).getStatus() == StreamStatus::ESTABLISHED) {

                Data data;
                int len = mxnet::read(inStream, &data, sizeof(data));
                long long now = mxnet::NetworkTime::now().get();

                if(len > 0) {
                    if(len == sizeof(data))
                    {
                        // latency sensor-actuator, counter needed to match with received sensor data
                        printf("[A] (%d,%d): L=%lld C=%u\n", sensorNodeID, actuatorNodeID, data.getLatency(), data.getCounter());
                        // latency controller-actuator (set into the sampleTime member of Data, for fields reusage)
                        long long latencyCA = data.getSampleTime();
                        printf("[A] (%d,%d): L=%lld C=%u\n", controllerNodeID, actuatorNodeID, latencyCA, data.getCounter());
                        
                        printf("[A] R (%d,%d) ID=%d NT=%lld C=%u V=%d\n",
                        id.src, id.dst, data.getId(), data.getNetworkTime(), data.getCounter(), data.getValue());
                        long long latency = now - data.getNetworkTime(); // latency from actuator to controller (computed on received message)
                        printf("[A] (%d,%d): L=%lld\n", id.src, id.dst, latency);
                    } else {
                        printf("[E] W (%d,%d) %d\n", id.src, id.dst, len);
                    }
                }
                else if(len == -1) {
                    print_dbg("[E] M (%d,%d)\n", id.src, id.dst);
                }
                else {
                    print_dbg("[E] M (%d,%d) Read returned %d\n", id.src, id.dst, len);
                }
            }
            
            closeStream(inStream);
            delete s;
        } catch(exception& e) {
            printf("exception %s\n",e.what());
        }
    }

private:
    const unsigned int streamOpeningDelay = 1000;
    MACContext *ctx;
    unsigned int counter = 0;
    TemperatureController tempController;
    float tempSetPoint = 500.f;

    int outStream = -1;
    bool outStreamOpened = false;

    int openServer(unsigned char port) {
        printf("[A] Opening server on port %d\n", port);
        /* Open a Server to listen for incoming streams */
        int server = listen(port,          // Destination port
                            serverParams); // Server parameters

        return server;
    }

    int openStream(unsigned char dest, unsigned char port) {
        // TODO può lanciare eccezioni?
        try {
            /* Open a Stream to another node */
            printf("[A] Opening stream to node %d\n", dest);
            int stream = connect(dest,                             // Destination node
                                    port,                          // Destination port
                                    clientParams);                 // Stream parameters 
                                    //1*ctx->getDataSlotDuration()); // Wakeup advance (max 1 tile)

            return stream;

        } catch(exception& e) {
            printf("exception %s\n",e.what());
            return -1;
        }
    }

    void openControllerOutputStream() {
        outStreamOpened = false;
        outStream = -1;
        while(!outStreamOpened) {
            outStream = openStream(actuatorNodeID, PORT1);
            if(outStream < 0) {                
                printf("[A] Stream opening failed! error=%d\n", outStream);
                Thread::sleep(5000);
            }
            else {
                outStreamOpened = true;
                printf("[A] Output stream opened to node %d\n", actuatorNodeID);
                //while(getInfo(outStream).getStatus() != StreamStatus::ESTABLISHED);
            }
        }
    }

    void closeStream(int stream) {
        if (stream == -1) {
            return;
        }

        StreamInfo info = getInfo(stream);
        //if (info != StreamInfo()) {
            printf("[A] Stream (%d,%d), status=", info.getSrc(), info.getDst());
            printStatus(getInfo(stream).getStatus());
            // NOTE: Remember to call close() after the stream has been closed remotely
            mxnet::close(stream);
        //}
    }

    void send(int stream, int controlVariable, long long sensorTimestamp, unsigned int counter) {
        // int res = mxnet::wait(stream);
        // if (res != 0) {
        //     printf("[A] Stream wait error\n");
        // }

        // Put last sensor data timestamp as a network time, so that
        // the actuator will be able to compute the end-to-end latency
        Data data(controllerNodeID, counter, sensorTimestamp, controlVariable);

        int ret = mxnet::write(stream, &data, sizeof(data));      
        if(ret >= 0) {
            printf("[A] Sent ID=%d NT=%lld C=%u\n",
                    data.getId(), data.getNetworkTime(), data.getCounter());
        }
        else
            printf("[E] Error sending data, result=%d\n", ret);
    }

    int runController(int s) {
        float cvFloat = tempController.step(s, tempSetPoint) * 60000.f; // control action has 60k steps
        int cv = static_cast<int>(cvFloat);
        printf("[A] S=%d CV=%d (= %.2f %%)\n", s, cv, cvFloat * 100.f / 60000.f);
        return cv;
    }

    static void processingSensorThreadLauncher(void *arg) {
        auto* s = reinterpret_cast<StreamThreadPar*>(arg);
        auto* c = reinterpret_cast<ControllerNode*>(s->obj);
        c->processSensor(s);
    }

    static void processingActuatorThreadLauncher(void *arg) {
        auto* s = reinterpret_cast<StreamThreadPar*>(arg);
        auto* c = reinterpret_cast<ControllerNode*>(s->obj);
        c->processActuator(s);
    }
};