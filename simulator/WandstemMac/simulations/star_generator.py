#!/usr/bin/python

import sys

print("""
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


network Star{0}
{{
    submodules:
        n0: RootNode {{
            address = 0;
        }}""".format(sys.argv[1],))

for i in xrange(1, int(sys.argv[1])):
    print("""        n{0}: Node {{
            address = {0};
        }}""".format(i,))
print("    connections:")
for i in xrange(1, int(sys.argv[1])):
    print("        n{0}.wireless++ <--> n0.wireless++;".format(i))
print("}")
