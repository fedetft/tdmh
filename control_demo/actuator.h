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

class Actuator {

public:
    Actuator(MACContext* c, StreamParameters sp, unsigned char p = 1) : ctx(c), serverParams(sp), port(p) {}

    void run() {
        while(!ctx->isReady()) {
            Thread::sleep(1000);
        }

        while(true) {
            // TODO make port parametric
            int server = openServer();
            if(server < 0) {                
                printf("[A] Server opening failed! error=%d\n", server);
                Thread::sleep(2000);
                continue;
            }

            try {
                while(getInfo(server).getStatus() == StreamStatus::LISTEN) {
                    int stream = accept(server);
                    Thread::create(&Actuator::receiveThreadLauncher, 1536, 
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
        //auto *s = reinterpret_cast<StreamThreadPar*>(arg);
        //int stream = s->stream;
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
                    actuate(data.getValue());
                    
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
        //delete s;
    }

private:
    MACContext *ctx;
    StreamParameters serverParams;
    unsigned char port = 0;
    int stream = -1;
    unsigned int counter = 0;
    int controlAction = 0;

    int openServer() {
        printf("[A] N=%d Waiting to authenticate master node\n", ctx->getNetworkId());
        while(waitForMasterTrusted()) {
            //printf("[A] N=%d StreamManager not present! \n", ctx->getNetworkId());
        }

        printf("[A] Opening server on port %d\n", port);
        /* Open a Server to listen for incoming streams */
        int server = listen(port,          // Destination port
                            serverParams); // Server parameters

        return server;
    }

    void actuate(int cv) {
        //printf("%d\n", d.getValue());
        controlAction = cv;
        printf("[A] Actuate: cv=%d\n", controlAction);
    }

    static void receiveThreadLauncher(void *arg) {
        auto* s = reinterpret_cast<StreamThreadPar*>(arg);
        auto* a = reinterpret_cast<Actuator*>(s->obj);
        a->receive(s->stream);
    }
};