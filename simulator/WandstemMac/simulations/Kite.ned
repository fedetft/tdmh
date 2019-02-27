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


network Kite
{
    parameters:
        n*.nodes = 8;
        n*.hops = 6;
    @display("bgb=517,501");
    submodules:
        n0: RootNode {
            address = 0;
            @display("p=260,37");
        }
        n1: Node {
            address = 1;
            @display("p=178,129");
        }
        n2: Node {
            address = 2;
            @display("p=340,129");
        }
        n3: Node {
            address = 3;
            @display("p=260,226");
        }
        n4: Node {
            address = 4;
            @display("p=260,345");
        }
        n5: Node {
            address = 5;
            @display("p=260,446");
        }
    connections:
        n0.wireless++ <--> n1.wireless++;
        n0.wireless++ <--> n2.wireless++;
        n2.wireless++ <--> n3.wireless++;
        n1.wireless++ <--> n3.wireless++;
        n3.wireless++ <--> n4.wireless++;
        n4.wireless++ <--> n5.wireless++;
}
