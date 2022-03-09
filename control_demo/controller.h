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
#include <control_demo/utils.h>

using namespace std;
using namespace mxnet;
using namespace miosix;

// TODO log both the sensor value received and the control action

class Controller {

public:
    Controller(MACContext* c, StreamParameters sp, StreamParameters cp,  unsigned char d, unsigned char p = 1) : 
                ctx(c), serverParams(sp), clientParams(cp) , dest(d), port(p) {}

    void run() {
        while(!ctx->isReady()) {
            Thread::sleep(1000);
        }

        // thread that accepts incoming connections
        Thread::create(&Controller::serverThreadLauncher, 2048, MAIN_PRIORITY, this);

        Thread::sleep(streamOpeningDelay);
        
        while(true) {
            // TODO make destination node and port parametric
            int stream = openStream(); // dest == 2 (controller) && port == 1
            if(stream < 0) {                
                printf("[A] Stream opening failed! error=%d\n", stream);
                Thread::sleep(2000);
                continue;
            }

            bool first = true;
            try {
                while(getInfo(stream).getStatus() == StreamStatus::ESTABLISHED) {
                    if(first) {
                        first=false;
                        printf("[A] Stream opened \n");
                    }

                    send(stream);
                }

                printf("[A] Stream (%d,%d) closed, status=", ctx->getNetworkId(), dest);
                printStatus(getInfo(stream).getStatus());
                // NOTE: Remember to call close() after the stream has been closed
                mxnet::close(stream);
            } catch(exception& e) {
                printf("exception %s\n",e.what());
            }
        }
    }

    void serverThread() {
        while(true) {
            int server = openServer();
            if(server < 0) {                
                printf("[A] Server opening failed! error=%d\n", server);
                Thread::sleep(2000);
                continue;
            }

            try {
                while(getInfo(server).getStatus() == StreamStatus::LISTEN) {
                    int stream = accept(server);
                    Thread::create(&Controller::receiveThreadLauncher, 1536, 
                                    MAIN_PRIORITY+1, new StreamThreadPar(this, stream));
                }
            } catch(exception& e) {
                printf("Unexpected exception while server accept: %s\n",e.what());
            } catch(...) {
                printf("Unexpected unknown exception while server accept\n");
            }
            printf("[A] Server on port %d closed, status=", port);
            printStatus(getInfo(server).getStatus());
            mxnet::close(server);
        }
    }

    void receive(int stream) {
        StreamInfo info = getInfo(stream);
        StreamId id = info.getStreamId();
        print_dbg("[A] Stream (%d,%d) accepted\n", id.src, id.dst);

        while(getInfo(stream).getStatus() == StreamStatus::ESTABLISHED) {
            Data data;
            int len = mxnet::read(stream, &data, sizeof(data));
            long long now = mxnet::NetworkTime::now().get();

            if(len > 0) {
                if(len == sizeof(data))
                {
                    sensorData = data.getValue();
                    controlAction = runController(sensorData);
                    
                    printf("[A] R (%d,%d) ID=%d T=%lld MH=%u C=%u\n",
                    id.src, id.dst, data.getId(), data.getTime(), data.getMinHeap(), data.getCounter());
                    long long delay = now - data.getNetworkTime();
                    printf("[A] (%d,%d): L=%lld\n", id.src, id.dst, delay);
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
        printf("[A] Stream (%d,%d) has been closed, status=", id.src, id.dst);
        printStatus(getInfo(stream).getStatus());
        // NOTE: Remember to call close() after the stream has been closed remotely
        mxnet::close(stream);
    }

private:
    const unsigned int streamOpeningDelay = 20000;
    MACContext *ctx;
    StreamParameters serverParams;
    StreamParameters clientParams;
    unsigned char port = 0;
    const unsigned char dest = 0;
    unsigned int counter = 0;
    int sensorData = 0;
    int controlAction = 0;

    int openServer() {
        // Not needed, the controller runs on the master
        // printf("[A] N=%d Waiting to authenticate master node\n", ctx->getNetworkId());
        // while(waitForMasterTrusted()) {
        //     //printf("[A] N=%d StreamManager not present! \n", ctx->getNetworkId());
        // }

        printf("[A] Opening server on port %d\n", port);
        /* Open a Server to listen for incoming streams */
        int server = listen(port,          // Destination port
                            serverParams); // Server parameters

        return server;
    }

    int openStream() {
        // TODO può lanciare eccezioni?
        try {
            // printf("[A] N=%d Waiting to authenticate master node\n", ctx->getNetworkId());
            // while(waitForMasterTrusted()) {
            //     //printf("[A] N=%d StreamManager not present! \n", ctx->getNetworkId());
            // }

            /* Open a Stream to another node */
            printf("[A] Opening stream to node %d\n", dest);
            int stream = connect(dest,                         // Destination node
                                port,                          // Destination port
                                clientParams,                  // Stream parameters 
                                2*ctx->getDataSlotDuration()); // Wakeup advance (max 1 tile)

            return stream;

        } catch(exception& e) {
            printf("exception %s\n",e.what());
            return -1;
        }
    }

    void send(int stream) {
        int res = mxnet::wait(stream);
        if (res != 0) {
            printf("[A] Stream wait error\n");
        }

        counter++;
        Data data(ctx->getNetworkId(), counter, controlAction);

        int ret = mxnet::write(stream, &data, sizeof(data));      
        if(ret >= 0) {
            printf("[A] Sent ID=%d Time=%lld NetTime=%lld MinHeap=%u Heap=%u Counter=%u\n",
                    data.getId(), data.getTime(), data.getNetworkTime(), data.getMinHeap(), data.getHeap(), data.getCounter());
        }
        else
            printf("[E] Error sending data, result=%d\n", ret);
    }

    int runController(int s) {
        //scanf("%d\n", &sampleValue);
        int cv = s * 2;
        printf("[A] Controller: s=%d cv=%d\n", s, cv);
        return cv;
    }

    static void receiveThreadLauncher(void *arg) {
        auto* s = reinterpret_cast<StreamThreadPar*>(arg);
        auto* c = reinterpret_cast<Controller*>(s->obj);
        c->receive(s->stream);
    }

    static void serverThreadLauncher(void *arg) {
        reinterpret_cast<Controller*>(arg)->serverThread();
    }
};