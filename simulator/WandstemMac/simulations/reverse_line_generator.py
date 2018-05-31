#!/usr/bin/python

import sys

def gen_ini(name):
    with open("{}.ini", 'w') as f:
        f.print("""[General]
network = {0}
eventlog-file = ${{resultdir}}/{0}.elog
experiment-label = {0}
sim-time-limit = 1000s
record-eventlog = true""".format(name))

def star_generator(quant):
    with open("Star{}.ned".format(quant), 'w') as f:
        f.print("""
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
                }}""".format(quant))

        for i in xrange(1, quant):
            f.print("""        n{0}: Node {{
                    address = {0};
                }}""".format(i))
        f.print("    connections:")
        for i in xrange(1, quant):
            f.print("        n{0}.wireless++ <--> n0.wireless++;".format(i))
        f.print("}")
    gen_ini("Star{}".format(quant))

def line_generator(quant):
    with open("Star{}.ned".format(quant), 'w') as f:
        f.print("""
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


        network Line{0}
        {{
            submodules:
                n0: RootNode {{
                    address = 0;
                }}""".format(quant))

        for i in xrange(1, quant):
            f.print("""        n{0}: Node {{
                    address = {0};
                }}""".format(i))
        f.print("    connections:")
        for i in xrange(0, quant - 1):
            f.print("        n{0}.wireless++ <--> n{1}.wireless++;".format(i, i+1))
        f.print("}")
    gen_ini("Line{}".format(quant))

def rline_generate(quant):
    with open("RLine{}.ned".format(quant), 'w') as f:
        f.print("""
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


        network ReverseLine{0}
        {{
            submodules:
                n0: RootNode {{
                    address = 0;
                }}""".format(quant))

        for i in xrange(1, quant):
            f.print("""        n{0}: Node {{
                    address = {0};
                }}""".format(i,))
        f.print("    connections:")
        f.print("        n0.wireless++ <--> n{0}.wireless++;".format(quant - 1))
        for i in xrange(1, quant - 1):
            f.print("        n{0}.wireless++ <--> n{1}.wireless++;".format(i, i+1))
        f.print("}")
    gen_ini("RLine{}".format(quant))



counts = [int(k) for k in sys.argv[1:]]

for i in counts:
    star_generator(i)
    line_generator(i)
    rline_generator(i)
