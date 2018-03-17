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


network FullMesh
{
    @display("bgb=591,501");
    submodules:
        n0: RootNode {
            address = 0;
            @display("p=263.61,211.09");
        }
        n1: Node {
            address = 1;
            @display("p=87.87,211.09");
        }
        n2: Node {
            address = 2;
            @display("p=148.47,321.18");
        }
        n3: Node {
            address = 3;
            @display("p=253.51,368.65");
        }
        n4: Node {
            address = 4;
            @display("p=382.79,350.47");
        }
        n5: Node {
            address = 5;
            @display("p=416.12,250.48");
        }
        n6: Node {
            address = 6;
            @display("p=422.18,161.6");
        }
        n7: Node {
            address = 7;
            @display("p=333.3,63.63");
        }
        n8: Node {
            address = 8;
            @display("p=214.12,63.63");
        }
        n9: Node {
            address = 9;
            @display("p=116.15,114.13");
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
        n1.wireless++ <--> n9.wireless++;
        n9.wireless++ <--> n8.wireless++;
        n7.wireless++ <--> n6.wireless++;
        n1.wireless++ <--> n8.wireless++;
        n9.wireless++ <--> n7.wireless++;
        n8.wireless++ <--> n6.wireless++;
        n7.wireless++ <--> n5.wireless++;
        n6.wireless++ <--> n4.wireless++;
        n5.wireless++ <--> n3.wireless++;
        n3.wireless++ <--> n1.wireless++;
        n3.wireless++ <--> n7.wireless++;
        n4.wireless++ <--> n9.wireless++;
        n4.wireless++ <--> n1.wireless++;
        n4.wireless++ <--> n2.wireless++;
        n5.wireless++ <--> n2.wireless++;
        n5.wireless++ <--> n1.wireless++;
        n5.wireless++ <--> n8.wireless++;
        n6.wireless++ <--> n3.wireless++;
        n6.wireless++ <--> n2.wireless++;
        n6.wireless++ <--> n1.wireless++;
        n6.wireless++ <--> n9.wireless++;
        n7.wireless++ <--> n2.wireless++;
        n7.wireless++ <--> n1.wireless++;
        n8.wireless++ <--> n2.wireless++;
        n9.wireless++ <--> n5.wireless++;
}
