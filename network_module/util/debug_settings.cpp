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

#include "debug_settings.h"
#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <queue>
#include <list>
#include <string>
#ifdef _MIOSIX
#include <miosix.h>
#include <unistd.h>
#include "stackrange.h"
#endif

using namespace std;
#ifdef _MIOSIX
using namespace miosix;
#endif

namespace mxnet {

#ifdef DEBUG_MESSAGES_IN_SEPARATE_THREAD
    
//#define WITH_PRINT_SEQNO

/*
 * Threaded logger configuration parameters
 */
const unsigned int maxMessageSize=128;  ///< Max size of individual debug message
const unsigned int maxQueueSize=5*1024; ///< Max size of queued logging data

class DebugPrinter
{
public:
    static DebugPrinter& instance();
    
    void enqueue(const string& s);
    
private:
    DebugPrinter(const DebugPrinter&) = delete;
    DebugPrinter& operator=(const DebugPrinter&) = delete;
    
    DebugPrinter() : thread(Thread::create(threadLauncher,2048,MAIN_PRIORITY,this)) {}
    
    void run();
    
    static void threadLauncher(void *argv);
    
    Thread *thread;
    FastMutex mutex;
    ConditionVariable cv;
    queue<string,list<string>> messages;
    unsigned int size=0;
    unsigned int maxSize=0;
#ifdef WITH_PRINT_SEQNO
    unsigned int seqNo=0;
#endif //WITH_PRINT_SEQNO
};

DebugPrinter& DebugPrinter::instance()
{
    static DebugPrinter singleton;
    return singleton;
}

void DebugPrinter::enqueue(const string& s)
{
    Lock<FastMutex> l(mutex);
#ifdef WITH_PRINT_SEQNO
    char buf[16];
    sprintf(buf,"%d ",seqNo++);
    string m=string(buf)+s;
    if(size + m.size() > maxQueueSize) return;
    messages.push(m);
    size += m.size();
    maxSize = max(maxSize,size);
    cv.signal();
#else //WITH_PRINT_SEQNO
    if(size + s.size() > maxQueueSize) return;
    messages.push(s);
    size += s.size();
    maxSize = max(maxSize,size);
    cv.signal();
#endif //WITH_PRINT_SEQNO
}

void DebugPrinter::run()
{
    printStackRange("DebugPrinter");
    const int logMaxSize = 10000; //Log max size every 10000 print_dbg
    int logCounter = 0;
    for(;;)
    {
        string s;
        {
            Lock<FastMutex> l(mutex);
            while(messages.empty()) cv.wait(l);
            s=messages.front();
            messages.pop();
            size -= s.size();
        }
        write(STDOUT_FILENO,s.c_str(),s.size());
        if(++logCounter >= logMaxSize)
        {
            logCounter = 0;
            printf("[L] log max size %d\n",maxSize);
        }
    }
}

void DebugPrinter::threadLauncher(void *argv)
{
    reinterpret_cast<DebugPrinter*>(argv)->run();
}

void print_dbg(const char *fmt, ...)
{
    va_list args;
    char str[maxMessageSize];
    va_start(args, fmt);
    vsnprintf(str, sizeof(str)-1, fmt, args);
    va_end(args);
    str[sizeof(str)-1]='\0';
    DebugPrinter::instance().enqueue(str);
}

#else //DEBUG_MESSAGES_IN_SEPARATE_THREAD

void print_dbg(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

#endif //DEBUG_MESSAGES_IN_SEPARATE_THREAD

void throwLogicError(const char *fmt, ...)
{
    va_list arg;
    char errorstr[128];
    va_start(arg, fmt);
    vsnprintf(errorstr, sizeof(errorstr)-1, fmt, arg);
    va_end(arg);
    errorstr[sizeof(errorstr)-1]='\0';
    throw logic_error(errorstr);
}

void throwRuntimeError(const char *fmt, ...)
{
    va_list arg;
    char errorstr[128];
    va_start(arg, fmt);
    vsnprintf(errorstr, sizeof(errorstr)-1, fmt, arg);
    va_end(arg);
    errorstr[sizeof(errorstr)-1]='\0';
    throw runtime_error(errorstr);
}

}
