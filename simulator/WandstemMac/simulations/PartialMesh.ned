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


network PartialMesh
{
    @display("bgb=591,501");
    submodules:
        n0: RootNode {
            address = 0;
            @display("p=279,98");
        }
        n1: Node {
            address = 1;
            @display("p=86,100");
        }
        n2: Node {
            address = 2;
            @display("p=104,203");
        }
        n3: Node {
            address = 3;
            @display("p=240,279");
        }
        n4: Node {
            address = 4;
            @display("p=350,272");
        }
        n5: Node {
            address = 5;
            @display("p=518,164");
        }
        n6: Node {
            address = 6;
            @display("p=467,74");
        }
        n7: Node {
            address = 7;
            @display("p=428,328");
        }
        n8: Node {
            address = 8;
            @display("p=304,379");
        }
        n9: Node {
            address = 9;
            @display("p=72,328");
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
        n2.wireless++ <--> n3.wireless++;
        n2.wireless++ <--> n9.wireless++;
        n3.wireless++ <--> n9.wireless++;
        n1.wireless++ <--> n2.wireless++;
        n3.wireless++ <--> n8.wireless++;
        n4.wireless++ <--> n8.wireless++;
        n3.wireless++ <--> n4.wireless++;
        n4.wireless++ <--> n7.wireless++;
        n8.wireless++ <--> n7.wireless++;
        n4.wireless++ <--> n5.wireless++;
        n5.wireless++ <--> n6.wireless++;
}
