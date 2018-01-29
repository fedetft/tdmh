/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   masterassignmentphase.h
 * Author: paolo
 *
 * Created on January 28, 2018, 9:54 PM
 */

#ifndef MASTERASSIGNMENTPHASE_H
#define MASTERASSIGNMENTPHASE_H

#include "assignmentphase.h"

namespace miosix {
    class MasterAssignmentPhase : public AssignmentPhase {
    public:
        MasterAssignmentPhase(long long reservationEndTime, SlotsNegotiator& negotiator) :
                AssignmentPhase(reservationEndTime, 0, 0, nullptr) {};
        MasterAssignmentPhase() = delete;
        MasterAssignmentPhase(const MasterAssignmentPhase& orig) = delete;
        virtual ~MasterAssignmentPhase();
        void execute(MACContext& ctx) override;
        void populatePacket();
    protected:
private:

};
}

#endif /* MASTERASSIGNMENTPHASE_H */

