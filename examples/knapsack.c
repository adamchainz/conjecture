#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>

#include "conjecture.h"

/*
This is testing a (bad) solution to the knapsack packing problem, based off the
principles outlined in
http://www.drmaciver.com/2015/07/properties-for-testing-optimisation/

It's here to demonstrate a couple things:

First, this is a fairly non-trivial data generation and testing problem. Some
of the data we generate depends on other data we generate in interesting ways -
e.g. we deliberately pick the capacity of the knapsack to be no larger than the
total weight of the items.

We also mix test execution and data generation: Note how in the middle of the
test we pick a random index from the ones that were chosen by the knapsack
selection algorithm.

This partly existed to see how well it simplifed. The answer is that it runs
into problems, but they're more or less the same sort of problems that
Hypothesis runs into. Certainly it doesn't simplify terribly.

*/

typedef struct {
  uint64_t weight;
  uint64_t value;
} knapsack_item;

int compare_item(const void *x, const void *y) {
  const knapsack_item *xi = x;
  const knapsack_item *yi = y;
  if(xi->value > yi->value)
    return -1;
  if(yi->value > xi->value)
    return 1;
  return 0;
}

typedef struct {
  size_t n_items;
  knapsack_item *items;
  uint64_t capacity;
} knapsack_problem;

void print_knapsack(knapsack_problem *problem) {
  printf("Knapsack. Capacity %zu. %" PRIu64 " candidates: ", problem->n_items,
         problem->capacity);
  for(size_t i = 0; i < problem->n_items; i++) {
    printf("(weight=%" PRIu64 ", value=%" PRIu64 ") ", problem->items[i].weight,
           problem->items[i].value);
  }
  printf("\n");
}

knapsack_problem *draw_knapsack(conjecture_context *context) {
  knapsack_problem *result =
      (knapsack_problem *)malloc(sizeof(knapsack_problem));
  result->n_items = 0;
  conjecture_variable_draw draw;
  conjecture_variable_draw_start(&draw, context, sizeof(knapsack_item));
  uint64_t total_weight = 0;
  while(conjecture_variable_draw_advance(&draw)) {
    knapsack_item *target =
        (knapsack_item *)conjecture_variable_draw_target(&draw);
    target->weight = conjecture_draw_small_uint64(context);
    total_weight += target->weight;
    target->value = conjecture_draw_small_uint64(context);
    result->n_items++;
  }
  result->items = (knapsack_item *)conjecture_variable_draw_complete(&draw);
  qsort(result->items, result->n_items, sizeof(knapsack_item), compare_item);

  result->capacity = conjecture_draw_uint64_under(context, total_weight);
  return result;
}

bool *solve_knapsack(knapsack_problem *problem) {
  bool *output = malloc(sizeof(bool) * problem->n_items);
  uint64_t capacity_left = problem->capacity;
  for(size_t i = 0; i < problem->n_items; i++) {
    assert(capacity_left >= 0);
    if(problem->items[i].weight <= capacity_left) {
      capacity_left -= problem->items[i].weight;
      output[i] = true;
    } else {
      output[i] = false;
    }
  }
  return output;
}

void test_increasing_weight_of_chosen_does_not_increase_score(
    conjecture_context *context, void *data) {
  knapsack_problem *problem = draw_knapsack(context);
  print_knapsack(problem);
  conjecture_assume(context, problem->n_items > 0);
  bool *chosen = solve_knapsack(problem);
  uint64_t old_value = 0;
  bool any_chosen = false;
  for(size_t i = 0; i < problem->n_items; i++) {
    if(chosen[i]) {
      old_value += problem->items[i].value;
      any_chosen = true;
    }
  }
  conjecture_assume(context, any_chosen);

  size_t selected_index;

  while(true) {
    size_t index =
        (size_t)conjecture_draw_uint64_under(context, problem->n_items - 1);
    if(chosen[index]) {
      selected_index = index;
      break;
    }
  }
  printf("Selected index: %zu\n", selected_index);
  if(problem->items[selected_index].weight > 0) {
    problem->items[selected_index].weight *= 2;
  } else {
    problem->items[selected_index].weight = 1;
  }
  bool *new_chosen = solve_knapsack(problem);
  uint64_t new_value = 0;
  for(size_t i = 0; i < problem->n_items; i++) {
    if(new_chosen[i]) {
      new_value += problem->items[i].value;
    }
  }
  printf("Initial score %" PRIu64 ", final score %" PRIu64 "\n", old_value,
         new_value);
  assert(new_value <= old_value);
}

int main() {
  conjecture_runner runner;
  conjecture_runner_init(&runner);
  runner.max_examples = 200;
  conjecture_run_test(
      &runner, test_increasing_weight_of_chosen_does_not_increase_score, NULL);
  conjecture_runner_release(&runner);
}
