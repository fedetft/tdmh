/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   lasthopassignmentphase.h
 * Author: paolo
 *
 * Created on January 29, 2018, 12:34 AM
 */

#ifndef LASTHOPASSIGNMENTPHASE_H
#define LASTHOPASSIGNMENTPHASE_H

#include "assignmentphase.h"

namespace miosix {
    class LastHopAssignmentPhase : public AssignmentPhase{
    public:
        LastHopAssignmentPhase(long long reservationEndTime, unsigned char myId, std::vector<unsigned char>* childrenIds) :
                AssignmentPhase(reservationEndTime, 1 /* TODO again, i need the maxhops constant */, myId, childrenIds) {};
        LastHopAssignmentPhase() = delete;
        LastHopAssignmentPhase(const LastHopAssignmentPhase& orig) = delete; 
        virtual ~LastHopAssignmentPhase();
        void execute(MACContext& ctx) override;
    private:

    };
}

#endif /* LASTHOPASSIGNMENTPHASE_H */

