/*
Copyright (C) 2015 David R. MacIver (david@drmaciver.com)

This file is part of Conjecture.

Conjecture is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Conjecture is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

If you wish to make use of it under another license, email
licensing@drmaciver.com to enquire about options.
*/

/* Conjecture is a new approach to property based testing that blends tests and
data generation.

A test function takes a conjecture_context, which contains state needed for
random data generation, maybe prints some information, and then either returns
a value, fails, or rejects the current conjecture_context. Both of the latter
will abort the current process (test cases are run under a fork), and anything
which aborts the process with a non-zero exit code will be considered a test
failure (e.g. anything which triggers assertions internal to your code).

A test case is a test function which returns void and takes only a single void*
data argument (this is opaque to Conjecture and is simply passed through).

Tests are then run as follows:

Random contexts are generated until one is found that fails, suppressing their
output. The context is then minimized through a series of operations designed
to produce simpler failures, until a minimal context is found.

The example is then re-executed with output no longer being suppressed, so you
can see the intermediate results that your program is printing.
*/

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

typedef enum {
  CONJECTURE_NO_RESULT,
  CONJECTURE_DATA_REJECTED,
  CONJECTURE_TEST_FAILED
} conjecture_test_status;

typedef struct {
  size_t capacity;
  size_t fill;
  unsigned char *data;
} conjecture_buffer;

typedef struct { bool rejected; } conjecture_comms;

typedef int64_t (*forker)(void *);

typedef struct {
  // Once this many examples have been tried and not rejected, conjecture will
  // declare the test to be passing.
  size_t max_examples;
  // Bound the internal memory usage of conjecture: A conjecture_context will
  // have a single buffer of at most this many bytes. Any test cases which
  // attempt to read past the end of this buffer will be rejected.
  size_t max_buffer_size;
  bool suppress_output;
  forker fork;
  void *fork_data;

  conjecture_comms *comms;

  conjecture_buffer *primary;
  conjecture_buffer *secondary;
  bool changed;
} conjecture_runner;

typedef struct {
  conjecture_runner *runner;
  conjecture_buffer *buffer;
  conjecture_test_status status;
  size_t current_index;
} conjecture_context;

bool conjecture_is_aborted(conjecture_context *context);

typedef void (*conjecture_test_case)(conjecture_context *context, void *data);

/*
  conjecture_run_test is the main entry point. Repeatedly run the provided test
  case in a subprocess until failure.

  If a failure is found, it will then be re-executed in the current process
  (this is expected to crash the process). If not, this function prints out
  some diagnostic information and then returns normally.
*/
void conjecture_run_test(conjecture_runner *runner,
                         conjecture_test_case test_case, void *data);

conjecture_buffer *
conjecture_run_test_for_buffer(conjecture_runner *runner,
                               conjecture_test_case test_case, void *data);

void conjecture_buffer_del(conjecture_buffer *b);

/*
  Initializes a runner with default settings and sets up the relevant
  communication channels.
*/
void conjecture_runner_init(conjecture_runner *runner);

/*
  Releases any resources associated with a runner.
*/
void conjecture_runner_release(conjecture_runner *runner);

void conjecture_context_init_from_buffer(conjecture_context *context,
                                         conjecture_runner *runner,
                                         conjecture_buffer *buffer);

/*

A note on test functions:

The two 'primitive' functions on which all else is built are
conjecture_reject and conjecture_draw_bytes. Everything else is built on top
of those.

All test functions consume bytes from the context and are documented as how
many they may consume.

Some functions consume a variable amount of bytes. In that case only bounds
will be documented.

All test functions will reject the example if they try to consume more bytes
than are available. Any other cases which will cause them to reject or fail
are explicitly documented.

*/

/*
  reject the current test.

  Consumes no bytes.
*/
void conjecture_reject(conjecture_context *context);

void conjecture_fail(conjecture_context *context);

/*
  reject the current test if requirement is false.

  Consumes no bytes.
*/
void conjecture_assume(conjecture_context *context, bool requirement);

/*
  Place n unsigned char values starting at destination, which must have enough
  space available.

  Consumes n bytes
*/
void conjecture_draw_bytes(conjecture_context *context, size_t n,
                           unsigned char *destination);

/*
  Draw a single bool

  Consumes 1 byte
*/
bool conjecture_draw_bool(conjecture_context *context);

/*
  Draw a single unsigned char

  Consumes 1 byte
*/
uint8_t conjecture_draw_uint8(conjecture_context *context);

/*
  Returns a zero terminated char*. This will always be non-NULL.
  The caller is responsible for passing it to free.

  Consumes >= 1 byte.
*/
char *conjecture_draw_string(conjecture_context *context);

/*
  Draw a single uint64_t

  Consumes 8 bytes
*/
uint64_t conjecture_draw_uint64(conjecture_context *context);

/*
  Draw a single uint64_t, biased heavily towards small numbers

  Consumes >= 1 byte.
*/
uint64_t conjecture_draw_small_uint64(conjecture_context *context);

/*
  Draw a uint64_t, x, such that 0 <= x <= max

  If max == 0 consumes no bytes, else consumes >= 8 bytes
*/
uint64_t conjecture_draw_uint64_under(conjecture_context *context,
                                      uint64_t max);

/*
  Draw an int64_t

  Consumes 8 bytes
*/
int64_t conjecture_draw_int64(conjecture_context *context);

/*
  Draw a double in the closed interval [0, 1].

  Consumes >= 9 bytes
*/
double conjecture_draw_fractional_double(conjecture_context *context);

/*
  Draw an arbitrary double (may include nan, infinity, etc).

  Consumes >= 1 bytes
*/
double conjecture_draw_double(conjecture_context *context);

/*
  conjecture_variable_draw lets you draw an unknown number of values and then
  gives you a pointer to the beginning of the block:

  You don't have to use it to do this of course, but by doing it this way you
  will tend to get better minimization of examples.

  Note that all pointers become invalid as soon as you call another
  conjecture_variable_draw function.

  The intended usage pattern is

  conjecture_variable_draw_start(&draw, context, sizeof(my_type));
  size_t length = 0;
  while(conjecture_variable_draw_advance(&draw)){
    my_type *target = (my_type*)conjecture_variable_draw_target(&draw);
    *target = some_draw_function(context);
    length++;
  }
  my_type *results = (my_type*)conjecture_variable_draw_complete(&draw);
*/

typedef struct {
  size_t object_size;
  conjecture_context *context;

  char *data;
  size_t attempts;
  size_t write_index;
  bool done;

  uint8_t threshold;
  size_t full_length;
} conjecture_variable_draw;

/*
  Begin a conjecture_variable_draw sequence for values of size object_size.

  Consumes >= 1 bytes.
*/
void conjecture_variable_draw_start(conjecture_variable_draw *variable,
                                    conjecture_context *context,
                                    size_t object_size);

/*
  Advances to the next location to write a value and returns true, or returns
  false if no value is available.

  Consumes 0 or 1 bytes
*/
bool conjecture_variable_draw_advance(conjecture_variable_draw *variable);

/*
  A pointer to where to write the next value if one is available, else NULL.
  The pointer is guaranteed to have object_size bytes available but may not
  have more than that.

  Consumes no bytes.
*/
char *conjecture_variable_draw_target(conjecture_variable_draw *variable);

/*
  Return a finished pointer to where the data has been written.

  Further use of variable is invalid without another call to start. The caller
  is responsible for freeing the returned memory.

  Consumes no bytes.
*/
char *conjecture_variable_draw_complete(conjecture_variable_draw *variable);
