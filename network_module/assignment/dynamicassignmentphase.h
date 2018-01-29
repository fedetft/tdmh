/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   dynamicassignmentphase.h
 * Author: paolo
 *
 * Created on January 28, 2018, 9:54 PM
 */

#ifndef DYNAMICASSIGNMENTPHASE_H
#define DYNAMICASSIGNMENTPHASE_H

#include "assignmentphase.h"

namespace miosix {
class DynamicAssignmentPhase : public AssignmentPhase {
public:
    AssignmentPhase(long long reservationEndTime, unsigned char hop, unsigned char myId, std::vector<unsigned char>* childrenIds) :
            AssignmentPhase(reservationEndTime, hop, myId, childrenIds) {};
    DynamicAssignmentPhase() = delete;
    DynamicAssignmentPhase(const DynamicAssignmentPhase& orig) = delete;
    virtual ~DynamicAssignmentPhase();
    void execute(MACContext& ctx) override;
private:

};
}

#endif /* DYNAMICASSIGNMENTPHASE_H */

