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

package wandstemmac.simulations;

import wandstemmac.Node;
import wandstemmac.RootNode;


network StarTopology
{
    @display("bgb=591,501");
    submodules:
        n0: RootNode {
            address = 0;
            @display("p=256,220");
        }
        n1: Node {
            address = 1;
            @display("p=63,222");
        }
        n2: Node {
            address = 2;
            @display("p=108,339");
        }
        n3: Node {
            address = 3;
            @display("p=217,401");
        }
        n4: Node {
            address = 4;
            @display("p=327,394");
        }
        n5: Node {
            address = 5;
            @display("p=426,321");
        }
        n6: Node {
            address = 6;
            @display("p=444,196");
        }
        n7: Node {
            address = 7;
            @display("p=387,86");
        }
        n8: Node {
            address = 8;
            @display("p=256,47");
        }
        n9: Node {
            address = 9;
            @display("p=141,96");
        }
    connections:
        n0.wireless++ <--> n1.wireless++;
        n0.wireless++ <--> n2.wireless++;
        n0.wireless++ <--> n3.wireless++;
        n0.wireless++ <--> n4.wireless++;
        n0.wireless++ <--> n5.wireless++;
        n0.wireless++ <--> n6.wireless++;
        n0.wireless++ <--> n7.wireless++;
        n0.wireless++ <--> n8.wireless++;
        n0.wireless++ <--> n9.wireless++;
}
