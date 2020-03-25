//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

package wandstemmac;

//
// A generic node in the Wandstem Mac.
// Synchronizes to a RootNode
//
simple Node
{
    parameters:
        int address;
        int nodes;
        int hops;
        // These allow to simulate nodes joining/node failure
        int connect_time = default(0);
        int disconnect_time = default(9223372036854775807); //numeric_limits<long long>::max()
        // Whether a node shall only join the network or also open a stream towards the master
        bool open_stream = default(true);
        @display("i=block/wrxtx");
    gates:
        inout wireless[];
}
