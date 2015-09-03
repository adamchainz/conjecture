#include <assert.h>
#include <inttypes.h>

#include "conjecture.h"

/*

This example will never actually accept a test context because it tries to read
too much data for it (8MB, which is more than the default maximum size).

*/

#define BUFFER_SIZE 8 * 1024 * 1024

void test_draws_too_much(conjecture_context *context, void *data){
  assert(data == NULL);
  unsigned char *buffer = malloc(BUFFER_SIZE);
  conjecture_draw_bytes(context, BUFFER_SIZE, buffer);
}

int main(){
  conjecture_runner runner;
  conjecture_runner_init(&runner);
  conjecture_run_test(&runner, test_draws_too_much, NULL);  
  conjecture_runner_release(&runner);
}
