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

// Created: 2019-8-23 for project WandstemMac

package wandstemmac.simulations;

import wandstemmac.Node;
import wandstemmac.RootNode;


network Office
{
    parameters:
        n*.nodes = 16;
        n*.hops = 6;
        @display("bgb=1158.2725,588.665");
    submodules:
        n0: RootNode {
            address = 0;
            @display("p=696.6575,254.09999");
        }
        n1: Node {
            address = 1;
            @display("p=696.6575,148.225");
        }
        n2: Node {
            address = 2;
            @display("p=153.51875,274.21625");
        }
        n3: Node {
            address = 3;
            @display("p=635.25,50.82");
        }
        n4: Node {
            address = 4;
            @display("p=210.69125,203.28");
        }
        n5: Node {
            address = 5;
            @display("p=635.25,364.21");
        }
        n7: Node {
            address = 7;
            @display("p=347.27,203.28");
        }
        n8: Node {
            address = 8;
            @display("p=90.9,258.56");
        }
        n9: Node {
            address = 9;
            disconnect_time = 40000000000;
            @display("p=347.27,411.85376");
        }
        n10: Node {
            address = 10;
            @display("p=281.6275,442.5575");
        }
        n11: Node {
            address = 11;
            @display("p=347.27,557.96124");
        }
        n12: Node {
            address = 12;
            @display("p=626.77997,557.96124");
        }
        n13: Node {
            address = 13;
            @display("p=635.25,459.4975");
        }
        n14: Node {
            address = 14;
            @display("p=487.025,203.28");
        }

    connections:
        n0.wireless++ <--> n1.wireless++;
        n0.wireless++ <--> n3.wireless++;
        n0.wireless++ <--> n4.wireless++;
        n0.wireless++ <--> n5.wireless++;
        n0.wireless++ <--> n7.wireless++;
        n0.wireless++ <--> n14.wireless++;

        n1.wireless++ <--> n3.wireless++;
        n1.wireless++ <--> n5.wireless++;
        n1.wireless++ <--> n7.wireless++;
        n1.wireless++ <--> n14.wireless++;

        n2.wireless++ <--> n4.wireless++;
        n2.wireless++ <--> n5.wireless++;
        n2.wireless++ <--> n8.wireless++;
        n2.wireless++ <--> n14.wireless++;

        n3.wireless++ <--> n5.wireless++;

        n4.wireless++ <--> n14.wireless++;
        n4.wireless++ <--> n7.wireless++;
        n4.wireless++ <--> n8.wireless++;

        n5.wireless++ <--> n7.wireless++;
        n5.wireless++ <--> n13.wireless++;
        n5.wireless++ <--> n13.wireless++;
        n5.wireless++ <--> n14.wireless++;

        n7.wireless++ <--> n8.wireless++;
        n7.wireless++ <--> n9.wireless++;
        n7.wireless++ <--> n10.wireless++;
        n7.wireless++ <--> n14.wireless++;

        n8.wireless++ <--> n14.wireless++;

        n9.wireless++ <--> n10.wireless++;
        n9.wireless++ <--> n14.wireless++;

        n10.wireless++ <--> n11.wireless++;
        n10.wireless++ <--> n14.wireless++;

        n11.wireless++ <--> n12.wireless++;
        n12.wireless++ <--> n13.wireless++;
}
