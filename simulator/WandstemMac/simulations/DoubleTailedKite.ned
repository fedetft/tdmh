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


network DoubleTailedKite
{
    @display("bgb=547.648,725.15204");
    submodules:
        n0: RootNode {
            address = 0;
            @display("p=225.664,61.920002");
        }
        n1: Node {
            address = 1;
            @display("p=225.664,162.368");
        }
        n2: Node {
            address = 2;
            @display("p=227.04001,260.064");
        }
        n3: Node {
            address = 3;
            @display("p=155.488,352.256");
        }
        n4: Node {
            address = 4;
            @display("p=297.216,352.256");
        }
        n5: Node {
            address = 5;
            @display("p=227.04001,452.704");
        }
        n6: RootNode {
            address = 6;
            @display("p=225.664,561.408");
        }
        n7: Node {
            address = 7;
            @display("p=225.664,661.856");
        }
    connections:
        n2.wireless++ <--> n3.wireless++;
        n2.wireless++ <--> n4.wireless++;
        n4.wireless++ <--> n5.wireless++;
        n3.wireless++ <--> n5.wireless++;
        n0.wireless++ <--> n1.wireless++;
        n1.wireless++ <--> n2.wireless++;
        n5.wireless++ <--> n6.wireless++;
        n6.wireless++ <--> n7.wireless++;
}
