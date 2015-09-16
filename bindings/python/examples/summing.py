import conjecture as c

def test_small_sum(context):
  assert(data == NULL);
  uint64_t x = c.draw_uint64_under(context, 10);
  uint64_t y = c.draw_uint64_under(context, 10);
  uint64_t z = c.draw_uint64_under(context, 10);
  printf("x=%d\n", (int)x);
  printf("y=%d\n", (int)y);
  printf("z=%d\n", (int)z);
  assert(x + y + z <= 25);
}

int main() {
  c.runner runner;
  c.runner_init(&runner);
  c.run_test(&runner, test_small_sum, NULL);
  c.runner_release(&runner);
}
