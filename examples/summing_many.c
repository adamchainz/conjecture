#include <assert.h>
#include <inttypes.h>

#include "conjecture.h"

void test_small_sum(conjecture_context *context, void *data){
  assert(data == NULL);
  conjecture_variable_draw draw;
  conjecture_variable_draw_start(&draw, context, sizeof(uint64_t));
  size_t length = 0;
  while(conjecture_variable_draw_advance(&draw)){
    uint64_t *target = (uint64_t*)conjecture_variable_draw_target(&draw);
    *target = conjecture_draw_uint64(context);
    length++;
  }
  uint64_t *results = (uint64_t*)conjecture_variable_draw_complete(&draw);
  uint64_t sum = 0;
  for(size_t i = 0; i < length; i++){
    conjecture_assume(context, sum < 0x8000000000000000LL);
    conjecture_assume(context, results[i] < 0x8000000000000000LL);
    sum += results[i];
  }
  printf("%zu examples: ", length);
  for(size_t i = 0; i < length; i++){
    printf("%" PRIu64 " ", results[i]);
  }
  printf("\n");
  if(length > 3)
    assert(sum <= 0x7000000000000000LL);
}

int main(){
  conjecture_runner runner;
  conjecture_runner_init(&runner);
  conjecture_run_test(&runner, test_small_sum, NULL);  
  conjecture_runner_release(&runner);
}
