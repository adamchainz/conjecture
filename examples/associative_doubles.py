"""
This is a sketch of using Conjecture from another language, in this case
Python using the CFFI module. It replicates the associative doubles example.

There will at some point be real bindings from Conjecture to Python, but this
for now serves to demonstrate that the conjecture API is one that's perfectly
reasonable to use from things that are not C.

Note that one way in which this *isn't* reasonable is that it currently
requires a failing test to exit the process. Conjecture needs to be better
factored so that this is not the case.
"""


from cffi import FFI
import os
import traceback
import functools
import math

ffi = FFI()
ffi.cdef("""
typedef struct {
  size_t capacity;
  size_t fill;
  unsigned char *data;
} conjecture_buffer;

typedef struct { bool rejected; } conjecture_comms;

typedef struct {
  conjecture_comms *comms;
  conjecture_buffer *buffer;
  size_t current_index;
} conjecture_context;

typedef struct {
  size_t max_examples;
  size_t max_buffer_size;

  conjecture_comms *comms;
} conjecture_runner;

typedef void (*conjecture_test_case)(conjecture_context *context, void *data);

void conjecture_runner_init(conjecture_runner *runner);
void conjecture_runner_release(conjecture_runner *runner);
void conjecture_run_test(conjecture_runner *runner,
                         conjecture_test_case test_case, void *data);
double conjecture_draw_double(conjecture_context *context);
void conjecture_assume(conjecture_context *context, bool requirement);
""")

C = ffi.dlopen(os.path.join(
    os.path.dirname(__file__), "..", "conjecture.so"))

def test(f):
    @ffi.callback(
        "void(conjecture_context*, void*)"
    )
    @functools.wraps(f)
    def runtest(context, data):
        try:
            f(context, data)
        except SystemExit as e:
            os._exit(e.code)
        except BaseException:
            traceback.print_exc()
            os._exit(1)
    return runtest

def declare(k, v):
    print("%s=%r" % (k, v))
    return v

@test
def a_test(context, data):
    x = declare('x', C.conjecture_draw_double(context))
    y = declare('y', C.conjecture_draw_double(context))
    z = declare('z', C.conjecture_draw_double(context))
    C.conjecture_assume(context, not math.isnan(x + y + z))
    assert (x + y) + z == x + (y + z)

if __name__ == '__main__':
    runner = ffi.new('conjecture_runner*')
    C.conjecture_runner_init(runner)
    C.conjecture_run_test(runner, a_test, ffi.NULL)
    C.conjecture_runner_release(runner)
