#include <assert.h>
#include <math.h>
#include <float.h>

#include "conjecture.h"

void test_small_sum(conjecture_context *context, void *data){
  assert(data == NULL);
  double x = conjecture_draw_double(context);
  double y = conjecture_draw_double(context);
  double z = conjecture_draw_double(context);
	printf("x=%.*f\n", FLT_DIG, x);
	printf("y=%.*f\n", FLT_DIG, y);
	printf("z=%.*f\n", FLT_DIG, z);
	conjecture_assume(context, !isnan(x + y + z));
	assert((x + y) + z == x + (y + z));
}

int main(){
  conjecture_runner runner;
  conjecture_runner_init(&runner);
  conjecture_run_test(&runner, test_small_sum, NULL);  
  conjecture_runner_release(&runner);
}
