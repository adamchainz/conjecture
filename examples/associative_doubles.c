#include <assert.h>
#include <math.h>
#include <float.h>

#include "conjecture.h"

/*

This test demonstrates floating point support. It generates
three doubles and tries to assert that floating point addition
is associative. Because it's not, this should fail.

*/

void test_associative_doubles(conjecture_context *context, void *data){
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
  conjecture_run_test(&runner, test_associative_doubles, NULL);  
  conjecture_runner_release(&runner);
}
