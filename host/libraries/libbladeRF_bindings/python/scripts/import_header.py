# Copyright (c) 2013-2018 Nuand LLC
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

__doc__ = """
Reads in a libbladeRF header file and produces a string ready for CFFI to
parse.

This is NOT a general-purpose tool; it should only be used on libbladeRF,
as it makes dangerous assumptions.
"""

from pycparser import parse_file, c_generator
import os
import sys
import textwrap

# This is the default for the Ubuntu python3-pycparser package
FAKE_LIBC_INCLUDE_DIR = "/usr/share/python3-pycparser/fake_libc_include"

# Lines matching these will be omitted from the output for brevity
STOPLIST = [
    "typedef struct Display Display;",
    "typedef unsigned long XID;",
    "typedef unsigned long VisualID;",
    "typedef XID Window;",
    "typedef void *MirEGLNativeWindowType;",
    "typedef void *MirEGLNativeDisplayType;",
    "typedef struct MirConnection MirConnection;",
    "typedef struct MirSurface MirSurface;",
    "typedef struct MirSurfaceSpec MirSurfaceSpec;",
    "typedef struct MirScreencast MirScreencast;",
    "typedef struct MirPromptSession MirPromptSession;",
    "typedef struct MirBufferStream MirBufferStream;",
    "typedef struct MirPersistentId MirPersistentId;",
    "typedef struct MirBlob MirBlob;",
    "typedef struct MirDisplayConfig MirDisplayConfig;",
    "typedef struct xcb_connection_t xcb_connection_t;",
    "typedef uint32_t xcb_window_t;",
    "typedef uint32_t xcb_visualid_t;",
]


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
                        r'-I{}'.format(os.path.dirname(filename)),
                        r'-I{}'.format(FAKE_LIBC_INCLUDE_DIR),
                        r'-D_DOXYGEN_ONLY_'])


def ast_to_c(ast):
    gen = MyCGenerator()
    return gen.visit(ast)


def generate_cdef(data, print_omitted_lines=False):
    for line in data.split('\n'):
        # Filter typedefs included from system headers
        if 'bladerf' not in line and ('typedef int' in line or
                                      'typedef _Bool' in line or
                                      line in STOPLIST):
            if print_omitted_lines:
                line = "/* Omitted: {} */".format(line)
            else:
                line = ""

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
        print("# Generated from {} by {}".format(
            os.path.basename(fn),
            os.path.basename(sys.argv[0])))
        print()
        print("header = \"\"\"")
        for l in generate_cdef(fs):
            print('  ' + '\n    '.join(textwrap.wrap(l)))
        print("\"\"\"")


if __name__ == '__main__':
    main()
