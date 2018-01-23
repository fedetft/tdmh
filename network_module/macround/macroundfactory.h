/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   macroundfactory.h
 * Author: paolo
 *
 * Created on December 6, 2017, 2:47 PM
 */

#ifndef MACROUNDFACTORY_H
#define MACROUNDFACTORY_H

namespace miosix {
    class MACRound;
    class MACContext;
    
    class MACRoundFactory {
    public:
        MACRoundFactory() {};
        virtual ~MACRoundFactory() {};
        MACRoundFactory& operator=(const MACRoundFactory& right) = delete;

        /**
         * Generates a new MACRound for bootstrapping the network.
         * @param ctx
         * @return 
         */
        virtual MACRound* create(MACContext& ctx) const = 0;
    };
}


#endif /* MACROUNDFACTORY_H */

