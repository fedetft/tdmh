
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


network ReverseLine8
{
    parameters:
        n*.nodes = 8;
        n*.hops = 7;
    submodules:
        n0: RootNode {
            address = 0;
        }
        n1: Node {
            address = 1;
        }
        n2: Node {
            address = 2;
        }
        n3: Node {
            address = 3;
        }
        n4: Node {
            address = 4;
        }
        n5: Node {
            address = 5;
        }
        n6: Node {
            address = 6;
        }
        n7: Node {
            address = 7;
        }
    connections:
        n0.wireless++ <--> n7.wireless++;
        n1.wireless++ <--> n2.wireless++;
        n2.wireless++ <--> n3.wireless++;
        n3.wireless++ <--> n4.wireless++;
        n4.wireless++ <--> n5.wireless++;
        n5.wireless++ <--> n6.wireless++;
        n6.wireless++ <--> n7.wireless++;
}