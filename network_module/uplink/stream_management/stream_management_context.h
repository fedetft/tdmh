/***************************************************************************
 *   Copyright (C)  2018 by Polidori Paolo                                 *
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

#include "stream_management_element.h"
#include "../../updatable_queue.h"
#include <vector>

namespace mxnet {

/**
 * Class for storing information about the streams
 */
class StreamManagementContext {
public:
    StreamManagementContext() {};
    virtual ~StreamManagementContext() {};

    /**
     * Receives a list of SME.
     */
    virtual void receive(std::vector<StreamManagementElement>& smes)=0;

    /**
     * Opens a new stream.
     */
    virtual void open(StreamManagementElement sme)=0;
};

class MasterStreamManagementContext : public StreamManagementContext {
public:
    MasterStreamManagementContext() : StreamManagementContext() {};
    ~MasterStreamManagementContext() {};

    /**
     * Just opens all the received streams.
     */
    virtual void receive(std::vector<StreamManagementElement>& smes);

    /**
     * Adds the stream, if missing.
     * Updates the data rate, if different.
     * Closes the stream, if the data rate is 0.
     */
     //TODO not yet used.
    virtual void open(StreamManagementElement sme);

    /**
     * Return SME object of given index
     */
    StreamManagementElement getStream(int index);

    /**
     * Return number of stream requests
     */
    int getStreamNumber();

    /**
     * Return vector of stream elements marked as established
     */
    std::vector<StreamManagementElement> getEstablishedStreams();

    /**
     * Return vector of stream elements marked as new
     */
    std::vector<StreamManagementElement> getNewStreams();

    /**
     * Return true if the stream list was modified since last time the flag was cleared 
     */
    bool wasModified() const {
      return modified_flag;
    };

    /**
     * Return true if any stream was removed since last time the flag was cleared 
     */
    bool wasRemoved() const {
        return removed_flag;
    };

    /**
     * Return true if any stream was added since last time the flag was cleared 
     */
    bool wasAdded() const {
        return added_flag;
    };

    /**
     * Set all flags to false
     */
    void clearFlags() {
        modified_flag = false;
        removed_flag = false;
        added_flag = false;
    };

    /**
     * Set the modified_flag to false
     */
    void clearModifiedFlag() {
        modified_flag = false;
    };


protected:
    std::vector<StreamManagementElement> opened;
    //IMPORTANT: this bit must be set to true whenever the data structure is modified
    bool modified_flag = false;
    bool removed_flag = false;
    bool added_flag = false;
};

class DynamicStreamManagementContext : public StreamManagementContext {
public:
    DynamicStreamManagementContext() : StreamManagementContext() {};
    ~DynamicStreamManagementContext() {};

    /**
     * Adds the received stream to the queue, or updates the existing if corresponding.
     */
    virtual void receive(std::vector<StreamManagementElement>& smes);

    /**
     * Adds the stream to the queue to be send and stores it in a pending list,
     * which makes a SME getting re-enqueued after it gets sent in an uplink message.
     */
    virtual void open(StreamManagementElement sme);

    /**
     * Sets a SME as opened, removing it from the list of pending.
     */
    void opened(StreamManagementElement sme);

    /**
     * Gets a count number of SMEs from the queue
     */
    std::vector<StreamManagementElement> dequeue(std::size_t count);
protected:
    UpdatableQueue<unsigned int, StreamManagementElement> queue;
    std::vector<StreamManagementElement> pending;
};

} /* namespace mxnet */
