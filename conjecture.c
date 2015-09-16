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

#include "conjecture.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <math.h>
#include <stdio.h>

#define BUFFER_STARTING_SIZE 8
#define CONJECTURE_EXIT 17

static conjecture_buffer *conjecture_buffer_new(size_t capacity) {
  conjecture_buffer *buffer =
      (conjecture_buffer *)malloc(sizeof(conjecture_buffer));
  buffer->capacity = capacity;
  buffer->fill = 0;
  buffer->data = (unsigned char *)malloc(capacity * sizeof(unsigned char));
  return buffer;
}

void conjecture_buffer_del(conjecture_buffer *b) {
  free(b->data);
  free(b);
}

void conjecture_fail(conjecture_context *context) { exit(CONJECTURE_EXIT); }

void conjecture_reject(conjecture_context *context) {
  if(context->comms != NULL) {
    context->comms->rejected = true;
    msync(context->comms, sizeof(conjecture_comms), 0);
    exit(EXIT_SUCCESS);
  } else {
    fprintf(stderr, "Rejected example when not running in subprocess\n");
  }
}

void conjecture_assume(conjecture_context *context, bool condition) {
  if(!condition)
    conjecture_reject(context);
}
void conjecture_draw_bytes(conjecture_context *context, size_t n,
                           unsigned char *destination) {
  if(n + context->current_index > context->buffer->fill) {
    conjecture_reject(context);
  }

  memmove(destination, context->buffer->data + context->current_index, n);
  context->current_index += n;
}

uint8_t conjecture_draw_uint8(conjecture_context *context) {
  unsigned char result;
  conjecture_draw_bytes(context, 1, &result);
  return result;
}

bool conjecture_draw_bool(conjecture_context *context) {
  return (bool)(conjecture_draw_uint8(context) & 1);
}

uint64_t conjecture_draw_uint64(conjecture_context *context) {
  unsigned char length = conjecture_draw_uint8(context) & 7;
  unsigned char buffer[8];
  conjecture_draw_bytes(context, 8, buffer);
  uint64_t result = 0;
  for(int i = 0; i <= length; i++) {
    result = (result << 8) + (uint64_t)buffer[i];
  }
  return result;
}

uint64_t conjecture_draw_small_uint64(conjecture_context *context) {
  uint64_t result = 0;
  while(true) {
    uint8_t datum = conjecture_draw_uint8(context);
    result += (uint64_t)datum;
    if(datum < 0xff)
      return result;
  }
}

char *conjecture_draw_string(conjecture_context *context) {
  size_t max_length = (size_t)conjecture_draw_small_uint64(context);
  char *data = malloc(max_length + 1);
  for(size_t i = 0; i < max_length; i++) {
    unsigned char c;
    conjecture_draw_bytes(context, 1, &c);
    data[i] = c;
    if(c == 0)
      return data;
  }
  data[max_length] = 0;
  return data;
}

static uint64_t saturate(uint64_t x) {
  x = x | (x >> 1);
  x = x | (x >> 2);
  x = x | (x >> 4);
  x = x | (x >> 8);
  x = x | (x >> 16);
  x = x | (x >> 32);
  return x;
}

uint64_t conjecture_draw_uint64_under(conjecture_context *context,
                                      uint64_t max_value) {
  if(max_value == 0) {
    return 0;
  }
  uint64_t mask = saturate(max_value);
  while(true) {
    uint64_t probe = mask & conjecture_draw_uint64(context);
    if(probe <= max_value) {
      return probe;
    }
  }
}

int64_t conjecture_draw_int64(conjecture_context *context) {
  return (int64_t)conjecture_draw_uint64(context);
}

#define MIN_INT64 -0x8000000000000000LL
#define MAX_INT64 0x7fffffffffffffffLL

int64_t conjecture_draw_int64_between(conjecture_context *context,
                                      int64_t lower, int64_t upper) {
  assert(lower <= upper);
  if(lower == upper)
    return lower;
  if((lower == MIN_INT64) && (upper == MAX_INT64))
    return conjecture_draw_int64(context);

  uint64_t minus_lower;
  if(lower == MIN_INT64) {
    minus_lower = 0x8000000000000000LL;
  } else {
    minus_lower = (int64_t)(-lower);
  }

  uint64_t gap;
  if(upper < 0) {
    gap = (uint64_t)(upper - lower);
  } else {
    gap = ((uint64_t)upper) + minus_lower;
  }

  uint64_t probe = conjecture_draw_uint64_under(context, gap);
  if(probe >= minus_lower) {
    return (int64_t)(probe - minus_lower);
  } else {
    return -(int64_t)(minus_lower - probe);
  }
}

double conjecture_draw_fractional_double(conjecture_context *context) {
  uint64_t a = conjecture_draw_uint64(context);
  if(a == 0)
    return 0.0;
  uint64_t b = conjecture_draw_uint64_under(context, a);
  return ((double)b) / ((double)a);
}

static double nasty_doubles[16] = {
    0.0, 0.5, 1.0 / 3, 10e6, 10e-6, 1.175494351e-38F, 2.2250738585072014e-308,
    1.7976931348623157e+308, 3.402823466e+38, 9007199254740992, 1 - 10e-6,
    1 + 10e-6, 1.192092896e-07, 2.2204460492503131e-016, INFINITY, NAN};

double conjecture_draw_double(conjecture_context *context) {
  // Start from the other end so that shrinking puts us out of the nasty zone
  uint8_t branch = 255 - conjecture_draw_uint8(context);
  if(branch < 32) {
    double base = nasty_doubles[branch & 15];
    if(branch & 16) {
      base = -base;
    }
    return base;
  } else {
    int64_t integral_part = conjecture_draw_int64(context);
    double fractional_part = conjecture_draw_fractional_double(context);
    return (double)integral_part + fractional_part;
  }
}

static bool is_failing_test_case(conjecture_runner *runner,
                                 conjecture_buffer *buffer,
                                 conjecture_test_case test_case, void *data) {
  conjecture_comms *comms = runner->comms;
  if(comms != NULL)
    comms->rejected = false;
  pid_t pid = (pid_t)runner->fork(runner->fork_data);
  if(pid == -1) {
    fprintf(stderr, "Unable to fork child process\n");
    exit(EXIT_FAILURE);
  } else if(pid == 0) {
    if(runner->suppress_output) {
      int devnull = open("/dev/null", O_WRONLY);
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
    }
    conjecture_context context;
    context.comms = comms;
    context.buffer = buffer;
    context.current_index = 0;
    test_case(&context, data);
    exit(0);
  } else {
    int status;
    waitpid(pid, &status, 0);
    return !WIFEXITED(status) || (WEXITSTATUS(status) != 0);
  }
}

int64_t standard_forker(void *ignored) { return (int64_t)fork(); }

void conjecture_runner_init(conjecture_runner *runner) {
  runner->max_examples = 200;
  runner->max_buffer_size = 1024 * 64;
  runner->fork = standard_forker;
  runner->fork_data = NULL;
  runner->suppress_output = true;
  int shmid = shmget(IPC_PRIVATE, sizeof(conjecture_comms), IPC_CREAT | 0666);
  if(shmid < 0) {
    fprintf(stderr, "Unable to create shared memory segment\n");
    exit(1);
  }
  errno = 0;
  void *shm = shmat(shmid, NULL, 0);
  assert(errno == 0);
  assert(shm != NULL);
  conjecture_comms *comms = (conjecture_comms *)shm;
  comms->rejected = false;
  runner->comms = comms;
}

void conjecture_runner_release(conjecture_runner *runner) {
  shmdt((char *)runner->comms);
}

static void buffer_copy(conjecture_buffer *destination,
                        conjecture_buffer *source) {
  assert(destination->capacity == source->capacity);
  destination->fill = source->fill;
  memmove(destination->data, source->data, source->fill);
}

static void buffer_delete_range(conjecture_buffer *buffer, size_t start,
                                size_t end) {
  assert(end >= start);
  if(end >= buffer->fill) {
    buffer->fill = start;
    return;
  }
  size_t gap = end - start;
  memmove(buffer->data + start, buffer->data + end, gap);
  buffer->fill -= gap;
}

static bool shrink_buffer(conjecture_buffer *destination,
                          conjecture_buffer *source, uint64_t stage) {
  buffer_copy(destination, source);
  for(size_t i = 0; i + 1 < source->fill; i++) {
    if(stage == 0) {
      destination->fill = i;
      return true;
    }
    stage--;
  }

  for(size_t i = 0; i < source->fill; i++) {
    for(size_t j = source->fill; j > i; j--) {
      if(stage == 0) {
        buffer_delete_range(destination, i, j);
        return true;
      }
      stage--;
    }
  }
  for(size_t i = 0; i < source->fill; i++) {
    for(size_t j = source->fill; j > i; j--) {
      if(stage == 0) {
        bool any_non_zero = false;
        for(size_t k = i; k < j; k++) {
          if(destination->data[k] > 0) {
            any_non_zero = true;
            destination->data[k] = 0;
          }
        }
        if(any_non_zero) {
          return true;
        } else {
          stage++;
        }
      }
      stage--;
    }
  }
  for(size_t i = 0; i < source->fill; i++) {
    for(unsigned char c = 0; c < source->data[i]; c++) {
      if(stage == 0) {
        destination->data[i] = c;
        return true;
      }
      stage--;
    }
  }
  for(size_t i = 0; i + 1 < source->fill; i++) {
    if(destination->data[i] > destination->data[i + 1]) {
      if(stage == 0) {
        unsigned char c = destination->data[i];
        destination->data[i] = destination->data[i + 1];
        destination->data[i + 1] = c;
        return true;
      }
      stage--;
    }
  }
  for(size_t i = 0; i + 1 < source->fill; i++) {
    if((destination->data[i] > 0) && (destination->data[i + 1] < 0xff)) {
      if(stage == 0) {
        destination->data[i] -= 1;
        destination->data[i + 1] += 1;
        return true;
      }
      stage--;
      if(stage == 0) {
        destination->data[i] -= 1;
        destination->data[i + 1] = 0xff;
        return true;
      }
      stage--;
    }
  }

  return false;
}

static void print_buffer(conjecture_buffer *buffer) {
  printf("[");
  for(size_t i = 0; i < buffer->fill; i++) {
    if(i > 0)
      printf("|");
    printf("%hhx", buffer->data[i]);
  }
  printf("]:%zu", buffer->fill);
}

void conjecture_context_init_from_buffer(conjecture_context *context,
                                         conjecture_buffer *buffer) {
  context->comms = NULL;
  context->buffer = buffer;
  context->current_index = 0;
}

conjecture_buffer *
conjecture_run_test_for_buffer(conjecture_runner *runner,
                               conjecture_test_case test_case, void *data) {
  conjecture_buffer *primary = conjecture_buffer_new(runner->max_buffer_size);

  size_t fill = 64;
  if(fill > runner->max_buffer_size) {
    fill = runner->max_buffer_size;
  }
  int good_examples = 0;
  int total_examples = 0;

  bool found_failure = false;

  FILE *urandom = fopen("/dev/urandom", "r");

  while((good_examples < runner->max_examples) &&
        (total_examples < 5 * runner->max_examples)) {
    good_examples++;
    total_examples++;
    primary->fill = fread(primary->data, 1, fill, urandom);
    if(is_failing_test_case(runner, primary, test_case, data)) {
      found_failure = true;
      break;
    }
    if(runner->comms->rejected) {
      fill *= 2;
      if(fill > runner->max_buffer_size) {
        fill = runner->max_buffer_size;
      }
      good_examples--;
    }
  }
  fclose(urandom);

  if(found_failure) {
    printf("Found failing test case after %d examples (%d accepted)\n",
           total_examples, good_examples);
    printf("Initial failing buffer: ");
    print_buffer(primary);
    printf("\n");
    conjecture_buffer *secondary =
        conjecture_buffer_new(runner->max_buffer_size);
    bool changed = true;
    int shrinks = 0;
    int extra_tries = 0;
    while(changed) {
      changed = false;
      size_t stage = 0;
      while(shrink_buffer(secondary, primary, stage++)) {
        extra_tries++;
        if(is_failing_test_case(runner, secondary, test_case, data)) {
          conjecture_buffer *tmp = primary;
          primary = secondary;
          secondary = tmp;
          changed = true;
          shrinks++;
          printf("Shrunk buffer to: ");
          print_buffer(primary);
          printf("\n");
          stage = 0;
          break;
        }
      }
    }
    conjecture_buffer_del(secondary);
    printf("Shrank example %d times in %d extra tries\n", shrinks, extra_tries);
    printf("Final buffer: ");
    print_buffer(primary);
    printf("\n");
    return primary;
  } else {
    printf("No failing test case after %d examples (%d accepted)\n",
           total_examples, good_examples);
    conjecture_buffer_del(primary);
    if(good_examples * 10 < total_examples) {
      printf("Failing test due to too few valid examples.\n");
      exit(EXIT_FAILURE);
    }
    return NULL;
  }
}

void conjecture_run_test(conjecture_runner *runner,
                         conjecture_test_case test_case, void *data) {
  conjecture_buffer *primary =
      conjecture_run_test_for_buffer(runner, test_case, data);
  if(primary != NULL) {
    conjecture_context context;
    conjecture_context_init_from_buffer(&context, primary);
    test_case(&context, data);
    printf("Flaky test! That was supposed to crash but it didn't.\n");
    exit(EXIT_FAILURE);
  }
}

void conjecture_variable_draw_start(conjecture_variable_draw *variable,
                                    conjecture_context *context,
                                    size_t object_size) {
  variable->object_size = object_size;
  variable->context = context;
  variable->attempts = 0;
  variable->write_index = 0xffffffffffffffff;
  variable->full_length = conjecture_draw_small_uint64(context);
  variable->done = false;
  if(variable->full_length > 0) {
    variable->threshold = conjecture_draw_uint8(context);
    variable->data = malloc(object_size * variable->full_length);
  } else {
    variable->threshold = 0;
    variable->data = NULL;
  }
}

bool conjecture_variable_draw_advance(conjecture_variable_draw *variable) {
  if(variable->done)
    return false;
  while(variable->attempts < variable->full_length) {
    variable->attempts++;
    if(conjecture_draw_uint8(variable->context) >= variable->threshold) {
      variable->write_index++;
      return true;
    } else {
      variable->done = true;
      return false;
    }
  }
  variable->done = true;
  return false;
}

char *conjecture_variable_draw_target(conjecture_variable_draw *variable) {
  if(variable->done)
    return NULL;
  else {
    return variable->write_index * variable->object_size + variable->data;
  }
}
char *conjecture_variable_draw_complete(conjecture_variable_draw *variable) {
  return variable->data;
}
