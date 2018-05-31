
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


network Line64
{
    parameters:
        n*.nodes = 64;
        n*.hops = 63;
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
        n8: Node {
            address = 8;
        }
        n9: Node {
            address = 9;
        }
        n10: Node {
            address = 10;
        }
        n11: Node {
            address = 11;
        }
        n12: Node {
            address = 12;
        }
        n13: Node {
            address = 13;
        }
        n14: Node {
            address = 14;
        }
        n15: Node {
            address = 15;
        }
        n16: Node {
            address = 16;
        }
        n17: Node {
            address = 17;
        }
        n18: Node {
            address = 18;
        }
        n19: Node {
            address = 19;
        }
        n20: Node {
            address = 20;
        }
        n21: Node {
            address = 21;
        }
        n22: Node {
            address = 22;
        }
        n23: Node {
            address = 23;
        }
        n24: Node {
            address = 24;
        }
        n25: Node {
            address = 25;
        }
        n26: Node {
            address = 26;
        }
        n27: Node {
            address = 27;
        }
        n28: Node {
            address = 28;
        }
        n29: Node {
            address = 29;
        }
        n30: Node {
            address = 30;
        }
        n31: Node {
            address = 31;
        }
        n32: Node {
            address = 32;
        }
        n33: Node {
            address = 33;
        }
        n34: Node {
            address = 34;
        }
        n35: Node {
            address = 35;
        }
        n36: Node {
            address = 36;
        }
        n37: Node {
            address = 37;
        }
        n38: Node {
            address = 38;
        }
        n39: Node {
            address = 39;
        }
        n40: Node {
            address = 40;
        }
        n41: Node {
            address = 41;
        }
        n42: Node {
            address = 42;
        }
        n43: Node {
            address = 43;
        }
        n44: Node {
            address = 44;
        }
        n45: Node {
            address = 45;
        }
        n46: Node {
            address = 46;
        }
        n47: Node {
            address = 47;
        }
        n48: Node {
            address = 48;
        }
        n49: Node {
            address = 49;
        }
        n50: Node {
            address = 50;
        }
        n51: Node {
            address = 51;
        }
        n52: Node {
            address = 52;
        }
        n53: Node {
            address = 53;
        }
        n54: Node {
            address = 54;
        }
        n55: Node {
            address = 55;
        }
        n56: Node {
            address = 56;
        }
        n57: Node {
            address = 57;
        }
        n58: Node {
            address = 58;
        }
        n59: Node {
            address = 59;
        }
        n60: Node {
            address = 60;
        }
        n61: Node {
            address = 61;
        }
        n62: Node {
            address = 62;
        }
        n63: Node {
            address = 63;
        }
    connections:
        n0.wireless++ <--> n1.wireless++;
        n1.wireless++ <--> n2.wireless++;
        n2.wireless++ <--> n3.wireless++;
        n3.wireless++ <--> n4.wireless++;
        n4.wireless++ <--> n5.wireless++;
        n5.wireless++ <--> n6.wireless++;
        n6.wireless++ <--> n7.wireless++;
        n7.wireless++ <--> n8.wireless++;
        n8.wireless++ <--> n9.wireless++;
        n9.wireless++ <--> n10.wireless++;
        n10.wireless++ <--> n11.wireless++;
        n11.wireless++ <--> n12.wireless++;
        n12.wireless++ <--> n13.wireless++;
        n13.wireless++ <--> n14.wireless++;
        n14.wireless++ <--> n15.wireless++;
        n15.wireless++ <--> n16.wireless++;
        n16.wireless++ <--> n17.wireless++;
        n17.wireless++ <--> n18.wireless++;
        n18.wireless++ <--> n19.wireless++;
        n19.wireless++ <--> n20.wireless++;
        n20.wireless++ <--> n21.wireless++;
        n21.wireless++ <--> n22.wireless++;
        n22.wireless++ <--> n23.wireless++;
        n23.wireless++ <--> n24.wireless++;
        n24.wireless++ <--> n25.wireless++;
        n25.wireless++ <--> n26.wireless++;
        n26.wireless++ <--> n27.wireless++;
        n27.wireless++ <--> n28.wireless++;
        n28.wireless++ <--> n29.wireless++;
        n29.wireless++ <--> n30.wireless++;
        n30.wireless++ <--> n31.wireless++;
        n31.wireless++ <--> n32.wireless++;
        n32.wireless++ <--> n33.wireless++;
        n33.wireless++ <--> n34.wireless++;
        n34.wireless++ <--> n35.wireless++;
        n35.wireless++ <--> n36.wireless++;
        n36.wireless++ <--> n37.wireless++;
        n37.wireless++ <--> n38.wireless++;
        n38.wireless++ <--> n39.wireless++;
        n39.wireless++ <--> n40.wireless++;
        n40.wireless++ <--> n41.wireless++;
        n41.wireless++ <--> n42.wireless++;
        n42.wireless++ <--> n43.wireless++;
        n43.wireless++ <--> n44.wireless++;
        n44.wireless++ <--> n45.wireless++;
        n45.wireless++ <--> n46.wireless++;
        n46.wireless++ <--> n47.wireless++;
        n47.wireless++ <--> n48.wireless++;
        n48.wireless++ <--> n49.wireless++;
        n49.wireless++ <--> n50.wireless++;
        n50.wireless++ <--> n51.wireless++;
        n51.wireless++ <--> n52.wireless++;
        n52.wireless++ <--> n53.wireless++;
        n53.wireless++ <--> n54.wireless++;
        n54.wireless++ <--> n55.wireless++;
        n55.wireless++ <--> n56.wireless++;
        n56.wireless++ <--> n57.wireless++;
        n57.wireless++ <--> n58.wireless++;
        n58.wireless++ <--> n59.wireless++;
        n59.wireless++ <--> n60.wireless++;
        n60.wireless++ <--> n61.wireless++;
        n61.wireless++ <--> n62.wireless++;
        n62.wireless++ <--> n63.wireless++;
}