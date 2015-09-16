from cffi import FFI
import os

ROOT = os.path.join(
    os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))

ffi = FFI()

with open(os.path.join(ROOT, "conjecture.c")) as source:
    SOURCE = source.read()

with open(os.path.join(ROOT, "conjecture.h")) as header:
    HEADER = header.read()

del source
del header

ffi.set_source(
    "conjecture._bindings",
    HEADER + '\n' + SOURCE.replace('#include "conjecture.h"', ''),
    extra_compile_args=["--std=c99"]
)

ffi.cdef(
    '\n'.join(
        l for l in
        HEADER.splitlines()
        if l[:1] != '#'
    ))

if __name__ == '__main__':
    ffi.compile()
