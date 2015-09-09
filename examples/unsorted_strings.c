#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "conjecture.h"

/*
This is an example demonstrating that we can generate null terminated strings.

It's also a proof of concept for how well we handle generating variable length
lists when the underlying data is highly variable in length: Shrinking a string
changes the stream of bytes a lot, which may heavily affect subsequent strings.

This doesn't seem to be a problem here, though it's possible that's a function
of the simplicity of the test.
*/

void test_ordered_strings(conjecture_context *context, void *data) {
  assert(data == NULL);
  conjecture_variable_draw draw;
  conjecture_variable_draw_start(&draw, context, sizeof(char *));
  size_t length = 0;
  while(conjecture_variable_draw_advance(&draw)) {
    char **target = (char **)conjecture_variable_draw_target(&draw);
    *target = conjecture_draw_string(context);
    length++;
  }
  char **results = (char **)conjecture_variable_draw_complete(&draw);
  printf("%zu examples: ", length);
  for(size_t i = 0; i < length; i++) {
    char *cs = results[i];
    printf("[");
    for(size_t j = 0;; j++) {
      char c = cs[j];
      if(c == 0) {
        if(i == length - 1)
          printf("]\n");
        else
          printf("], ");
        break;
      } else {
        printf("%hhx,", c);
      }
    }
  }
  for(size_t i = 0; i < length; i++) {
    for(size_t j = i + 1; j < length; j++) {
      assert(strcmp(results[i], results[j]) <= 0);
    }
  }
}

int main() {
  conjecture_runner runner;
  conjecture_runner_init(&runner);
  conjecture_run_test(&runner, test_ordered_strings, NULL);
  conjecture_runner_release(&runner);
}
