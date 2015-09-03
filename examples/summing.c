#include <assert.h>
#include <inttypes.h>

#include "conjecture.h"

void test_small_sum(conjecture_context *context, void *data){
  assert(data == NULL);
  uint64_t x = conjecture_draw_uint64_under(context, 10);
  uint64_t y = conjecture_draw_uint64_under(context, 10);
  uint64_t z = conjecture_draw_uint64_under(context, 10);
  printf("x=%d\n", (int)x);
  printf("y=%d\n", (int)y);
  printf("z=%d\n", (int)z);
  assert(x + y + z <= 25);
}

int main(){
  conjecture_runner runner;
  conjecture_runner_init(&runner);
  conjecture_run_test(&runner, test_small_sum, NULL);  
  conjecture_runner_release(&runner);
}
