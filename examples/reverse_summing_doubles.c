#include <assert.h>
#include <inttypes.h>
#include <math.h>

#include "conjecture.h"

/*
This is an example demonstrating mixing doubles with variable width
list generation.

The reason this is interesting is that the amount of data consumed by
drawing a double is potentially quite variable, so there's a lot of
scope for things to go wrong when you try shrinking them because it
can cause adjacent doubles to sortof smoosh together.

This turns out to not be a major problem. The examples produced
are generally quite simple. Here's some example output:

  3 examples: 3.000000 2.000000 36028797018963968.000000
  sum=36028797018963976.000000, revsum=36028797018963968.000000

It also demonstrates one of the various problems with floating point
addition - that reversing a list can radically change the value.

*/

void test_sum_is_reversible(conjecture_context *context, void *data) {
  assert(data == NULL);
  conjecture_variable_draw draw;
  conjecture_variable_draw_start(&draw, context, sizeof(double));
  size_t length = 0;
  while(conjecture_variable_draw_advance(&draw)) {
    double *target = (double *)conjecture_variable_draw_target(&draw);
    *target = conjecture_draw_double(context);
    length++;
  }
  double *results = (double *)conjecture_variable_draw_complete(&draw);
  double sum = 0;
  double revsum = 0;
  for(size_t i = 0; i < length; i++) {
    conjecture_assume(context, sum < 0x8000000000000000LL);
    conjecture_assume(context, results[i] < 0x8000000000000000LL);
    sum += results[i];
    revsum += results[length - 1 - i];
  }
  printf("%zu examples: ", length);
  for(size_t i = 0; i < length; i++) {
    printf("%f ", results[i]);
  }
  printf("\n");
  conjecture_assume(context, !isnan(sum));
  printf("sum=%f, revsum=%f\n", sum, revsum);
  assert(sum <= revsum + 1);
  assert(revsum <= sum + 1);
}

int main() {
  conjecture_runner runner;
  conjecture_runner_init(&runner);
  conjecture_run_test(&runner, test_sum_is_reversible, NULL);
  conjecture_runner_release(&runner);
}
