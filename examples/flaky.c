#include <assert.h>
#include <sys/types.h>
#include <unistd.h>

#include "conjecture.h"

/*

This is an example of a flaky test. The test will pass when run in
the parent process but fail when run in any other process. Because our test
generation and simplification runs in a child process this test will always
fail during the initial stages and then pass when we do the final run.

Conjecture solves this problem by erroring anyway and warning you that your
test is flaky.
*/

void test_i_am_calling_process(conjecture_context *context, void *data) {
  pid_t *mypid = (pid_t*)data;
  assert(getpid() == *mypid);
}

int main() {
  conjecture_runner runner;
  conjecture_runner_init(&runner);
  pid_t whoami = getpid();
  conjecture_run_test(&runner, test_i_am_calling_process, &whoami);
  conjecture_runner_release(&runner);
}
