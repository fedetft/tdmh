
#include <cassert>
#include "mac_context.h"
#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/power_manager.h"

using namespace std;
using namespace miosix;

void stub() { assert(false); }

namespace mxnet {

void MACContext::sendAt(const void* pkt, int size, long long ns) { stub(); }
RecvResult MACContext::recv(void* pkt, int size, long long timeout, Transceiver::Correct c) { stub(); }
    
}

namespace miosix {

long long getTime() { stub(); }

void Thread::nanoSleepUntil(long long when) { stub(); }

void PowerManager::deepSleep(long long delta) { stub(); }
void PowerManager::deepSleepUntil(long long when) { stub(); }
    
}
