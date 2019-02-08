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


network SimReal2018RTSS
{
    parameters:
        n*.nodes = 16;
        // NOTE: A lower number of hops triggers a bug in the MAC Implementation
        n*.hops = 6;
    @display("bgb=1148.8619,502.944");
    submodules:
        n0: RootNode {
            address = 0;
            @display("p=971.412,429.936");
        }
        n1: Node {
            address = 1;
            @display("p=923.75397,278.85");
        }
        n2: Node {
            address = 2;
            @display("p=65.909996,429.936");
        }
        n3: Node {
            address = 3;
            @display("p=1027.182,88.218");
        }
        n4: Node {
            address = 4;
            @display("p=348.816,364.026");
        }
        n5: Node {
            address = 5;
            @display("p=653.016,364.026");
        }
        n6: Node {
            address = 6;
            @display("p=65.909996,154.128");
        }
        n7: Node {
            address = 7;
            @display("p=502.944,398.50198");
        }
        n8: Node {
            address = 8;
            @display("p=242.346,438.048");
        }
    connections:
        n2.wireless++ <--> n4.wireless++;
        n4.wireless++ <--> n5.wireless++;
        n0.wireless++ <--> n1.wireless++;
        n1.wireless++ <--> n3.wireless++;
        n0.wireless++ <--> n3.wireless++;
        n5.wireless++ <--> n0.wireless++;
        n5.wireless++ <--> n1.wireless++;
        n5.wireless++ <--> n3.wireless++;
        n5.wireless++ <--> n7.wireless++;
        n5.wireless++ <--> n8.wireless++;
        n7.wireless++ <--> n0.wireless++;
        n7.wireless++ <--> n1.wireless++;
        n7.wireless++ <--> n8.wireless++;
        n7.wireless++ <--> n4.wireless++;
        n7.wireless++ <--> n2.wireless++;
        n4.wireless++ <--> n8.wireless++;
        n2.wireless++ <--> n8.wireless++;
        n2.wireless++ <--> n6.wireless++;
        n6.wireless++ <--> n8.wireless++;
}
