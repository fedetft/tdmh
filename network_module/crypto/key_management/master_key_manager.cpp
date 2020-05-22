#include "master_key_manager.h"

namespace mxnet {

AesGcm& MasterKeyManager::getUplinkGCM() {
}

AesGcm& MasterKeyManager::getTimesyncGCM() {
}

AesGcm& MasterKeyManager::getScheduleDistributionGCM() {
}

void MasterKeyManager::startRekeying() {
}

void MasterKeyManager::applyRekeying() {
}

void* MasterKeyManager::getMasterKey() {
}

void* MasterKeyManager::getNextMasterKey() {
}

unsigned int MasterKeyManager::getMasterIndex() {
}

} //namespace mxnet
