#!/usr/bin/env python3
#
# Reads in a libbladeRF header file and produces a string ready for CFFI to
# parse.
#
# This is NOT a general-purpose tool; it should only be used on libbladeRF,
# as it makes dangerous assumptions.

from pycparser import parse_file, c_generator
import os
import sys
import textwrap

# This is the default for the Ubuntu python3-pycparser package
FAKE_LIBC_INCLUDE_DIR = "/usr/share/python3-pycparser/fake_libc_include"


class MyCGenerator(c_generator.CGenerator):
    # Clean up enumeration printouts...
    def _generate_enum_body(self, members):
        # `[:-2] + '\n'` removes the final `,` from the enumerator list
        return ''.join(self.visit(value) for value in members)[:-2] + '\n'

    def _generate_enum(self, n):
        """ Generates code for structs, unions, and enums. name should be
            'struct', 'union', or 'enum'.
        """
        members = () if n.values is None else n.values.enumerators
        s = 'enum ' + (n.name or '')
        if members:
            s += '\n'
            s += self._make_indent()
            self.indent_level += 2
            s += '{\n'
            s += self._generate_enum_body(members)
            self.indent_level -= 2
            s += self._make_indent() + '}'
        return s

    def visit_Enumerator(self, n):
        val = self.visit(n.value)
        return '{indent}{name}{equals}{value},\n'.format(
            indent=self._make_indent(),
            name=n.name,
            equals=" = " if val else "",
            value=val,
        )

    def visit_Enum(self, n):
        return self._generate_enum(n)


def parse_header(filename):
    return parse_file(filename,
                      use_cpp=True,
                      cpp_args=[
                        r'-I{}'.format(FAKE_LIBC_INCLUDE_DIR),
                        r'-D_DOXYGEN_ONLY_'])


def ast_to_c(ast):
    gen = MyCGenerator()
    return gen.visit(ast)


def generate_cdef(data, print_omitted_lines=False):
    for line in data.split('\n'):
        # Filter typedefs included from system headers
        if 'bladerf' not in line and ('typedef int' in line or
                                      'typedef _Bool' in line):
            if print_omitted_lines:
                line = "/* Omitted: {} */".format(line)
            else:
                line = ""

        # Fix indentation...
        line = line.replace("  ", "    ")

        # If we have an expression like:
        #   char magic[BLADERF_IMAGE_MAGIC_LEN + 1];
        # We need to boil it down to:
        #   char magic[8];
        if '[' in line and ']' in line:
            bstart = line.find('[')+1
            bend = line.find(']')
            prestuff = line[:bstart]
            poststuff = line[bend:]
            stuff = line[bstart:bend]
            # XXX: this is wicked dangerous to run on untrusted code
            evalstuff = str(eval(stuff))

            newline = prestuff + evalstuff + poststuff

            if newline != line:
                line = "{} /* Original: {}{}{} */".format(
                    newline, prestuff.strip(), stuff, poststuff)

        if line:
            yield line.rstrip()


def main():
    if len(sys.argv) < 2:
        print("usage: {} headerfile.h [headerfile.h...]".format(sys.argv[0]))
        sys.exit(1)

    for fn in sys.argv[1:]:
        f = parse_header(fn)
        fs = ast_to_c(f)
        print("    /* Generated from {} by {} */".format(
            os.path.basename(fn),
            os.path.basename(sys.argv[0])))

        for l in generate_cdef(fs):
            print('    ' + '\n        '.join(textwrap.wrap(l)))


if __name__ == '__main__':
    main()
