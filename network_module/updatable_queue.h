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

#include <utility>
#include <map>
#include <deque>
#include <algorithm>
#include <stdexcept>

namespace mxnet {

/**
 * A queue data structure in which the elements are enqueued with a relative key, and they can be updated
 * preserving the queue ordering of the old value. Elements must be unique by key.
 * @tparam keyType the type of the key to which the element is associated.
 * @tparam valType the type of the values stored in the queue.
 */
template<class keyType, class valType>
class UpdatableQueue {
public:
    UpdatableQueue() {
        // TODO Auto-generated constructor stub

    }
    virtual ~UpdatableQueue() {
        // TODO Auto-generated destructor stub
    }
    /**
     * Extracts the next element in the queue.
     * @return the next element in the queue.
     */
    valType&& dequeue();
    /**
     * Returns the next element in the queue without removing it.
     * @return the next element in the queue.
     */
    valType& top() const;
    /**
     * Adds an element to the queue.
     * @param key unique reference for the value.
     * @param val the value to be added.
     */
    bool enqueue(keyType key, const valType& val);
    /**
     * Removed an element based on its key, if present.
     * @param key the key for which the value needs to be removed.
     * @return if the element was present and removed
     */
    bool removeElement(keyType key);
    /**
     * Updates the value associated with a certain key.
     * If no value is present, no modification will occur.
     * @param key the identifier for the value.
     * @param val the new value to be associated with the key.
     * @return if any value was updated
     */
    bool update(keyType key, const valType& val);
    /**
     * Returns an element by its key. An exception is thrown is no value is associated with such key
     * @param key the key identifying the value.
     * @return the value associated with the key.
     */
    valType& getByKey(keyType key);
    /**
     * Returns an element by its key. An exception is thrown is no value is associated with such key
     * @param key the key identifying the value.
     * @return the value associated with the key.
     */
    const valType& getByKey(keyType key) const;
    /**
     * Checks whether an element related to the provided key is present.
     * @param key the key for which the value presence is checked.
     * @return if the value is present.
     */
    bool hasKey(keyType key) const;
    /**
     * Checks if any element is present in the queue.
     * @return if any element is present.
     */
    bool isEmpty();

    /** Returns the size of the %UpdatableQueue.  */
    std::size_t size() const { return data.size(); }
private:
    std::map<keyType, valType> data;
    std::deque<keyType> queue;
};

template<class keyType, class valType>
valType&& UpdatableQueue<keyType, valType>::dequeue() {
    if (queue.empty()) throw std::runtime_error("no element in queue");
    auto key = queue.back();
    queue.pop_back();
    auto retval = data[key];
    data.erase(key);
    return std::move(retval);
}

template<class keyType, class valType>
valType& UpdatableQueue<keyType, valType>::top() const {
    if (queue.empty()) throw std::runtime_error("no element in queue");
    return data[queue.back()];
}

template<class keyType, class valType>
bool UpdatableQueue<keyType, valType>::enqueue(keyType key, const valType& val) {
    if(hasKey(key)) return false;
    data[key] = val;
    queue.push_front(key);
    return true;
}

template<class keyType, class valType>
bool UpdatableQueue<keyType, valType>::removeElement(keyType key) {
    if(!hasKey(key)) return false;
    data.erase(key);
    queue.erase(std::find(queue.begin(), queue.end(), key));
    return true;
}

template<class keyType, class valType>
bool UpdatableQueue<keyType, valType>::update(keyType key, const valType& val) {
    if(!hasKey(key)) return false;
    data[key] = val;
    return true;
}

template<class keyType, class valType>
inline const valType& UpdatableQueue<keyType, valType>::getByKey(keyType key) const {
    if (!hasKey(key)) throw std::runtime_error("empty");
    return data.at(key);
}

template<class keyType, class valType>
inline valType& UpdatableQueue<keyType, valType>::getByKey(keyType key) {
    if (!hasKey(key)) throw std::runtime_error("empty");
    return data.at(key);
}

template<class keyType, class valType>
inline bool UpdatableQueue<keyType, valType>::hasKey(keyType key) const {
    return data.count(key);
}

template<class keyType, class valType>
bool UpdatableQueue<keyType, valType>::isEmpty() {
    return data.size() == 0;
}

} /* namespace mxnet */
