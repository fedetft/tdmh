/***************************************************************************
 *   Copyright (C)  2018, 2019 by Polidori Paolo, Federico Terraneo        *
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

#include <map>
#include <list>
#include <functional>
#include <stdexcept>

namespace mxnet {

/**
 * A queue data structure in which the elements are enqueued with a relative key,
 * and they can be updated preserving the queue ordering of the old value.
 * Elements are thus kept unique by key.
 * \tparam K the type of the key to which the element is associated.
 * \tparam V the type of the values stored in the queue.
 */
template<typename K, typename V>
class UpdatableQueue
{
public:
    /**
     * Adds an element to the queue, replacing a previous element with the
     * same key, if present.
     * \param key unique reference for the value
     * \param val the value to be added/replaced
     */
    void enqueue(K key, const V& val);
    
    /**
     * Adds an element to the queue, replacing a previous element with the
     * same key, if present.
     * \param key unique reference for the value
     * \param val the value to be added/replaced
     */
    void enqueue(K key, V&& val);
    
    /**
     * Adds an element to the queue, replacing a previous element with the
     * same key, if present.
     * \param key unique reference for the value
     * \param val the value to be added
     * \param f function that is called if the element to be added replaces a
     * previous element
     */
    void enqueue(K key, const V& val, std::function<void (V& oldVal, const V& newVal)> f);
    
    /**
     * \return the oldest element in the queue, without removing it
     */
    V& top() const;
    
    /**
     * \return the oldest element in the queue, removing it
     */
    V dequeue();

    /**
     * \return a pair comprised of the oldest element in the queue, which is
     * removed, and its key.
     */
    std::pair<K,V> dequeuePair();
    
    /**
     * Remove all elements in the queue
     */
    void clear()
    {
        data.clear();
        queue.clear();
    }
    
    /**
     * \return true if the queue is empty
     */
    bool empty() const { return data.empty(); }
    
    /**
     * \return the number of elements
     */
    std::size_t size() const { return data.size(); }
    
private:
    std::map<K,V> data;
    std::list<K> queue;
};

template<typename K, typename V>
void UpdatableQueue<K,V>::enqueue(K key, const V& val)
{
    auto it = data.find(key);
    if(it != data.end())
    {
        it->second = val; //Replace
    } else {
        data[key] = val;
        queue.push_front(key);
    }
}

template<typename K, typename V>
void UpdatableQueue<K,V>::enqueue(K key, V&& val)
{
    auto it = data.find(key);
    if(it != data.end())
    {
        it->second = std::move(val); //Replace
    } else {
        data.insert(std::make_pair(key, std::move(val)));
        queue.push_front(key);
    }
}

template<typename K, typename V>
void UpdatableQueue<K,V>::enqueue(K key, const V& val, std::function<void (V& oldVal, const V& newVal)> f)
{
    auto it = data.find(key);
    if(it != data.end())
    {
        f(it->second, val);
        it->second = val; //Replace
    } else {
        data.insert(std::make_pair(key, val));
        queue.push_front(key);
    }
}

template<typename K, typename V>
V& UpdatableQueue<K,V>::top() const
{
    if(data.empty()) throw std::runtime_error("no element in queue");
    return data[queue.back()];
}

template<typename K, typename V>
V UpdatableQueue<K,V>::dequeue()
{
    if(data.empty()) throw std::runtime_error("no element in queue");
    auto key = queue.back();
    auto it = data.find(key);
    auto result = std::move(it->second); //Move out and rely on RVO
    queue.pop_back();
    data.erase(it);
    return result;
}

template<typename K, typename V>
std::pair<K,V> UpdatableQueue<K,V>::dequeuePair()
{
    if(data.empty()) throw std::runtime_error("no element in queue");
    auto key = queue.back();
    auto it = data.find(key);
    auto value = std::move(it->second); //Move out and rely on RVO
    queue.pop_back();
    data.erase(it);
    std::pair<K,V> result = std::pair<K,V>(key, value);
    return result;
}


} // namespace mxnet
