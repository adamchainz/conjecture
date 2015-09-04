#include <assert.h>
#include <inttypes.h>

#include "conjecture.h"

/*

This example draws three small integers, adds them together, and asserts that
their sum is never too large.

It will usually fail (probability of it not in 200 runs is very low), and when
it does it should shrink all three numbers down to some triple of integers
which sums to 26.

*/

void test_small_sum(conjecture_context *context, void *data) {
  assert(data == NULL);
  uint64_t x = conjecture_draw_uint64_under(context, 10);
  uint64_t y = conjecture_draw_uint64_under(context, 10);
  uint64_t z = conjecture_draw_uint64_under(context, 10);
  printf("x=%d\n", (int)x);
  printf("y=%d\n", (int)y);
  printf("z=%d\n", (int)z);
  assert(x + y + z <= 25);
}

int main() {
  conjecture_runner runner;
  conjecture_runner_init(&runner);
  conjecture_run_test(&runner, test_small_sum, NULL);
  conjecture_runner_release(&runner);
}
