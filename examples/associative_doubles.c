#include <assert.h>

#include "conjecture.h"

void test_small_sum(conjecture_context *context, void *data){
  assert(data == NULL);
  double x = conjecture_draw_fractional_double(context);
  double y = conjecture_draw_fractional_double(context);
  double z = conjecture_draw_fractional_double(context);
  printf("x=%f\n", x);
  printf("y=%f\n", y);
  printf("z=%f\n", z);
	assert((x + y) + z == x + (y + z));
}

int main(){
  conjecture_runner runner;
  conjecture_runner_init(&runner);
  conjecture_run_test(&runner, test_small_sum, NULL);  
  conjecture_runner_release(&runner);
}
