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


network SimpleTopology
{
    @display("bgb=515,404");
    submodules:
        n0: RootNode {
            @display("p=260,37");
        }
        n1: Node {
            @display("p=178,129");
        }
        n2: Node {
            @display("p=340,129");
        }
        n3: Node {
            @display("p=260,226");
        }
    connections:
        n0.wireless++ <--> n1.wireless++;
        n0.wireless++ <--> n2.wireless++;
        n1.wireless++ <--> n3.wireless++;
        n2.wireless++ <--> n3.wireless++;
}
