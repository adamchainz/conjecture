===================
What is Conjecture?
===================

The core concept of Conjecture is this: What if you could do the most basic sort of randomized testing which
everybody starts out with, only instead of it being awful it was actually amazing?

In a Conjecture test, you are given a random number generator, you generate some data off it (there are lots of
helper functions to help you generate the sort of data you want), you print some things out to indicate what it
is your program actually did, and then as if by magic all the right random choices are taken to produce as
simple an example as possible.

Then it remembers that example for later so when you rerun the test it refinds it immediately.

On top of that, Conjecture offers a composable library of data generators you can use to produce the data you
want from that.


High level idea
~~~~~~~~~~~~~~~

The hard part of property based testing is shrinking. You implement a data generator, and then in order to get
good output you need to implement shrinkers for that data.

An idea that has been around for a while (it is implemented in Erlang QuickCheck, test.check and Hypothesis at the
minimum) but was absent from the original QuickCheck is that in many cases you can get shrinking 'for free' by
building your generators on top of existing ones and then just shrinking the inputs. For example, in Hypothesis
if you wanted to produce a generator for sorted lists, you could do:

.. code-block:: python

  def sorted_lists(elements):
      return lists(elements).map(sorted)

This then implements shrinking by shrinking the source list then applying the sort function.

As long as your functions map simpler inputs (according to the shrinking order of the source strategy) to simpler
outputs (according to whatever your preference is), this will produce good results.

The key observation of Conjecture is that there is a single class of base strategies under which you can reasonably
build everything else: Fixed size blocks of bytes, shrinking lexicographically when viewed as a sequence of
unsigned integers in [0, 256).

For example, the following is a good generator for 64-bit integers:

.. code-block:: python

  def int64s():
    return byteblock(8).map(lambda b: int.from_bytes(b, 'big', signed=True))

This will produce an order where positive is simpler than negative, and otherwise closer to zero is simpler
(from_bytes with signed uses twos complement).

So all that is needed is a good shrinker for blocks of bytes and everything else follows.

This then provides a solution to another problem: What do you do about dependence between generators?

In both Hypothesis and test.check (and I think Erlang QuickCheck) you can perform a monadic bind and still get
shrinking. For example:

.. code-block:: python

  def constant_lists(elements):
      return elements.flatmap(lambda x: lists(just(x)))

This produces a strategy which generates lists which are just duplicates of the same value: It first draws a value
from elements, then given that value it draws a value from lists(just(x)).

How should this be implemented? Particularly given that the strategy can change radically based on the input value:

.. code-block:: python

  booleans().flatmap(lambda x: text() if x else floats())

We need to be able to perform shrinks that can shrink both the left hand and the right hand side of the flatmap,
but shrinks on the left can completely invalidate prior shrinks on the right.

In test.check this is handled by just redrawing whenever you shrink the left. Hypothesis has a slightly more effective
strategy, but it's a horror show of mutability and careful management of state (there are technical reasons why
test.check's approach couldn't work for Hypothesis, mainly around the permissibility of side-effects in the
function passed to flatmap).

In Conjecture we follow Hypothesis's lead of using mutability, but fortunately the emphasis on bytes allows for
a much simpler approach:

When drawing data for Conjecture, we draw a single large block of bytes up front. Our basic n byte strategy then
just reads the next n bytes off the block and updates an index into it to point to after where it's read. Draws
which try to read past the end of the buffer are marked as invalid.

This reduces the entire process of simplification to one of simplifying a block of bytes. As well as meaning that
we don't have to ever implement our own shrinkers, this turns out to have a number of advantages.

However, this requires a number of further refinements to work well.

Refinement 1: Output simplification
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The next important insight is this: We only actually care about what is printed out by the test when it runs
(e.g. in Hypothesis it prints 'falsifying example', the repr of the example, and then by an exception stack
trace'). This then becomes a text string that we are trying to simplify.

*Usually* we expect shrinking the input to shrink the output, but the dependencies can be complicated: For example,
deleting a byte might cause boundaries to shift such that the output suddenly became radically more complicated.

Thus what we do is we record an output stream as a sequence of bytes. We assume that these bytes represent utf-8
text, or at least some ascii-compatible format. If they don't this won't 'break' anything but may result in
unexpected preferences.

We then define a preference order over outputs. Shrinks which result in an increase in that order are rejected (
shrinks which produce the same output are accepted).

The order is as follows:

1. Fewer bytes is always better. More bytes is always worse.
2. Given two outputs of the same length, we compare them in lexicographic order of pleasingness of their bytes.

Where 'pleasingness' is a reordering of the bytes from 0 to 256 to reorder the first 127 bytes in an order that
produces good ascii output. The following is the current chosen ordering of the ASCII characters:

.. code-block:: python

  CHR_ORDER = [
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
      'A', 'a', 'B', 'b', 'C', 'c', 'D', 'd', 'E', 'e', 'F', 'f', 'G', 'g',
      'H', 'h', 'I', 'i', 'J', 'j', 'K', 'k', 'L', 'l', 'M', 'm', 'N', 'n',
      'O', 'o', 'P', 'p', 'Q', 'q', 'R', 'r', 'S', 's', 'T', 't', 'U', 'u',
      'V', 'v', 'W', 'w', 'X', 'x', 'Y', 'y', 'Z', 'z',
      ' ',
      '_', '-', '=', '~',
      '"', "'",
      ':', ';', ',', '.', '?', '!',
      '(', ')', '{', '}', '[', ']', '<', '>',
      '*', '+', '/', '&', '|', '%',
      '#', '$', '@',  '\\', '^', '`',
      '\t', '\n', '\r',
      '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07', '\x08',
      '\x0b', '\x0c', '\x0e', '\x0f', '\x10', '\x11', '\x12', '\x13', '\x14',
      '\x15', '\x16', '\x17', '\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d',
      '\x1e', '\x1f',
  ]


The ASCII reordering is not *strictly* necessary but produces nicer output by prioritising less messy
characters and avoiding 'weird' control characters in the output where possible.

As well as helpfully avoiding cases where the shrinks are unexpectedly bad, this has a few nice properties:

1. We are always at the current best solution, so we may implement a timeout without worrying that we're at a
   point which is worse than we previously were.
2. It improves performance in some cases because it allows us to stop the shrink earlier when there are still
   valid shrinks left but they don't actually improve matters.

Note: Although we only focus on the aesthetics of ascii, this works perfectly well for unicode output. When
outputting in utf-8, all codepoints >= 127 will be ordered by code-point, because the order of length then
bytewise lexicographical does the right thing here.

Refinement 2: Manual costing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Sometimes the lexicographical ordering of examples isn't quite what we want, so we provide a mechanism for
manually hinting that something is worse than it looks. For example NaN is shorter and thus simpler than 10000.0,
but we would much rather have the latter. When we had manually written shrinkers we could do this ourselves, but
when shrinking comes entirely from the source input we have no acccess to the structure of values. Thus another
mechanism is needed.

To solve this we provide a lightweight hinting mechanism in the form of an incur_cost() function. This lets us
impose a positive integer cost. 

Prior to comparing output, we compare cost. A strictly lower cost is always better, a strictly higher cost is
always worse, then with equal costs we fall back to output.

In an original version, cost had the output length added to it - so for example 100 bytes of output and a cost of
20 would be worse than a cost of 30 and 50 bytes of output. This turns out to be a bad idea because there can be
a complicated relation between draws and data. The following example revealed this:

.. code-block:: python

  find(booleans().flatmap(lambda x: lists(just(x))), lambda x: len(x) >= 10).

This is looking for a list consisting of the same boolean repeated multiple times which is of length at least
10.

According to the cost order, we want to find a list consisting of 10 False values. But False has one more
character than True, so the byte length of the output here is 10 more. This can't be compensated for by any
reasonable cost difference because the boolean is only drawn once! Hence the change to comparing costs first.


Refinement 3: Automatic pruning
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We want to be quite aggressive about reducing the amount of data consumed, because doing so produces better
examples and makes future shrinking easier.

One way of presenting this is paying attention to any opportunity to shrink sizes: Whenever we've performed a
successful shrink, we automatically prune the buffer to the amount of data read. This means that even operations
that aren't designed to delete data can end up shrinking the size of the buffer.

For example, suppose we had the following sequence of operations:

.. code-block:: python

    a = draw_byte(data)
    n = draw_byte(data)
    block = draw_bytes(data, n)

Suppose we were to try a sequence of shrinks that took a, b from (255, 100) -> (255, 10) -> (254, 100).

These are all lexicographically valid, but the middle one causes us to draw fewer bytes, so the buffer
was automatically pruned. This makes the second shrink invalid because it will now probably cause us to read past
the end of the buffer.

Refinement 4: Interval marking
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The most important shrinking operations are ones that can delete data so as to reduce the size of the example
outputs: This is both true because smaller examples are better, and also because smaller data lets us focus more
effectively on shrinking what's left.

We can do this quite well by deleting intervals from the underlying byte buffer and seeing if it works: If we
design our collection generators well then this will delete individual elements (the key here is to generate a
"stopping" value which indicates that you've reached the end of the collection).

But there's a problem: This is really expensive, because there are O(n^2) intervals to try deleting. Heuristics
can help, but it's hard to balance deleting enough with spending too much time failing to delete.

So the solution is another hinting mechanism: We provide markers start_example() and stop_example() which let
you mark the boundaries of where examples live. These can be arbitrarily nested.

The end result is that we produce a series of intervals that are likely to be useful to try to delete. These
automatically include every basic block of bytes that we draw, but also can include the boundary of more
complex structures. In particular, the following sketches how you can use it for drawing lists:

.. code-block:: python

  def draw_list(data, draw_element):
      end_marker = draw_byte(data)
      result = []
      while True:
          start_example(data)
          stopping = draw_byte(data)
          if stopping > end_marker:
              stop_example(data)
              break
          result.append(draw_element)
          stop_example(data)
      return result

This allows both a shrink at the beginning which with automatic pruning will probably cause us to read less data,
but it also allows for elements in the middle of the list: If an interval containing a (stopping, example) pair is
deleted, we just skip that iteration of the loop.

Shrinker implementation
~~~~~~~~~~~~~~~~~~~~~~~

The shrinker takes a record of a previous test run and tries various replacement buffers. All of these are either
strictly shorter or the same length and lexicographically before the previous buffer.

When a buffer is considered, it is run through the test. If it is still interesting, its output and cost are
examined and if they are not worse than that of the previous then the current best is replaced.

Note: 'Not worse' rather than 'strictly better' is a deliberate decision made on the basis of some examples where
it can take multiple shrink passes to improve the example which don't initially appear to affect the result.

The design of the shrinker is in terms of passes of increasing difficulty. If a pass fails to make any changes,
we proceed on to the next one. Otherwise once the pass has completed we start again from the beginning. The
early passes tend to be linear in the size of the buffer, the latter quadratic, with the hope being that by the
time we make it to the quadratic passes the buffer is small enough that we can afford that.

We iterate until make it through all the passes with no successful shrinks, or until we hit one of the stopping
points. The stopping points are:

1. Once we have exceeded a certain run time
2. Once we have exceeded a certain number of successful shrinks (this tends to be a sign that we're not usefully
   making progress).

The exact set of shrink passes is still under fairly active experimentation. Here is a snapshot of what they were
at the time of this writing:

The shrink passes are:

1. For each interval, delete that interval.
2. For i from 0 to 7, try unsetting the i'th bit on every byte in the buffer.
3. For each interval, sort that interval bytewise.
4. For each byte that appears more than once in the output, and each strictly smaller byte, try replacing all
   occurrences of that byte with the smaller byte.
5. For each index, and each byte strictly smaller than the byte at that index, try replacing the byte at that 
   index with that byte in the following 3 ways:

   a) Leaving all other indices unchanged
   b) Replace the byte at the next index with 255
   c) Randomize the rest of the array
6. For each adjacent pair of bytes, if they are out of order swap them.
7. For each zero byte in the buffer, try replacing it and every zero byte to the left of it up until the previous
   non-zero byte with 255 and decrement the non-zero byte by one.
8. For each pair of indices with the same bytewise value, try replacing them with each smaller byte if they are 
   non-zero. If they are zero, if their preceding indices are both non-zero try replacing the pair with 255 and
   subtracting one from their predecessor.

Some of these may seem slightly weird and arbitrary. They've typically been picked with specific examples in mind.
In particular, the pair based ones are designed to allow for simultaneous simplification.

This appears to strike a good balance between performance and quality. In particular it seems to be able to reach
a lot of levels of example quality that are almost impossible with a classic QuickCheck, albeit at the cost of a
somewhat worse runtime.

Two interesting examples from the example quality tests:

.. code-block:: python

  def test_containment():
      u, v = find(
          st.tuples(intlist, st.n_byte_unsigned(8)),
          lambda uv: uv[1] in uv[0] and uv[1] >= 100
      )
      assert u == [100]
      assert v == 100

This is essentially unachievable to a classic QuickCheck: It wouldn't find any examples at all (see the next
section for why Conjecture can) and if you somehow conspired to let it it wouldn't
be able to do simultaneous shrinking of the two. Hypothesis can do simultaneous shrinking, but can only do it within
lists. In Conjecture simultaneous shrinking will happen regardless of any structural relation between the examples,
because it happens at the byte level so the relation is irrelevant.

.. code-block:: python


  def test_minimize_sets_of_sets():
      sos = st.lists(st.lists(st.n_byte_unsigned(8)).map(frozenset)).map(set)

      def union(ls):
          return reduce(operator.or_, ls, frozenset())

      x = find(sos, lambda ls: len(union(ls)) >= 30)
      assert x == {frozenset(range(30))}

This example tries to find a set of sets with at least 30 elements in their union. In QuickCheck or Hypothesis
this would probably result in multiple disjoint sets whose union was the range 0 to 30 (hopefully), but they've
got almost no hope of finding a single element example. In Conjecture, as a consequence of how lists are represented
at the byte level, adjacent lists can be merged together which produces a better example.

This isn't a particularly interesting thing in its own right, but it demonstrates the sort of shrinking that can
happen when you don't have to care abotu the structure.


Test execution details
----------------------

The abstract API for running Conjecture doesn't know anything about tests. It provides a TestData object, which
exposes the following operations:

1. draw_bytes(n) -> draws n bytes from the buffer, or sets the status to overrun and terminates the test.
2. start_example/stop_example -> mark example boundaries
3. incur_cost(c) -> add c to the cost of this example
4. mark_invalid -> set the status of the test data to invalid and terminate the test
5. mark_interesting -> set the status of the test data to interesting and terminate the test.

A TestData object can be in one of four states:

1. Overrun: The test tried to read past the end of the buffer.
2. Invalid: The test marked the data as invalid at some point
3. Valid: Nothing to see here.
4. Interesting: This is the sort of thing we want to see 

The goal is to find an interesting TestData object. In aid of this we have a TestRunner, which takes some settings
and a function that is passed a TestData. It then generates data until it gets bored or one of them produces a
status of interesting.

If we find an interesting TestData we proceed to shrinking as per above, otherwise we stop the test with no result.

Quality of initial data is currently worse than Hypothesis (which has features designed to be good at correlated
output) but still pretty good. It's also able to do some interesting things that would otherwise be impossible in
normal QuickCheck.

Data is generated in fixed size blocks (this is configurable if you need to generate larger examples but defaults
to 8K), drawn uniformly at random. It is then immediately passed to the test function.

We then proceed through a series of mutations. A mutation takes the existing buffer and changes it in some way.
The test is then run. If it would lower the status of the example (e.g. valid to invalid, invalid to overrun,
etc the result is discarded). Otherwise it is accepted.

A fixed number of mutations (10 by default) is tried, then if we haven't found an interesting buffer we start
again from the beginning.

After we've found 200 valid examples or 1000 examples total (again, configurable) without any being interesting
we stop the test.

The exact mutations tried are still a work in progress, but the current set is:

If the current status is overrun:

The goal here is to make the input smaller, which should happen if you reduce the byte values and the strategy
is well designed (if not there's not much you can do). The only mutation operation while overrun is that you take
each byte, and with equal probability you replace it with 0, replace it with a random value in [0, b] or leave it
alone.

This should usually get you into a non-overrun state pretty quickly, moving on to the next set of mutations:

If the current status is valid or invalid:

1. Pick a random index in the read section of the buffer and change the byte there - with equal probabilities,
   replace it with 0, 255 or a new random byte.
2. Pick a random interval and clone it over another random interval.

The second one allows some interesting possibilities because you can do things like have complicated dependencies
between examples. This is why the test_containment example from above works.

This needs more work: It's hard to get a good balance between finding desirable data with high probability and
not getting stuck in boring areas of the search space.

Design goals
~~~~~~~~~~~~

There are two design goals for this stage, neither of which are currently  well met, both of which are trying to
claim back some of the benefits of Hypothesis data generation over a pure QuickCheck:

1. Correlated output: When Hypothesis generates a list it generates interesting correlations amongst the elements,
   so it can 
2. Adaptive assume: Hypothesis satisfies assumptions much more than pure chance would suggest, using its parameter
   system. The fact that transitions from valid to invalid states are not permitted here *should* help achieve that,
   but this needs more investigation.

Frequently Asked/Anticipated Questions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

How is this different from Quickcheck style testing?
----------------------------------------------------

For starters it has most of the benefits that Hypothesis does over classic Quickcheck. In particular it is
possible to serialize arbitrary examples after minimization, it works transparently with tests that try to
perform side effects or mutate the values you've passed in, and you can chain data generation together while
retaining simplification.

Its two main advantages over Hypothesis from a usage point of view are:

1. You can mix test execution and data generation freely. For example, if you perform a calculation in your
   test which returns a list of values and then pick an arbitrary value from that list, that's a random choice
   subject to simplification like any other (it will simplify towards having picked the first element of the
   list). In Hypothesis or Quickcheck there's quite a distinct separation between test execution and example
   generation. Hypothesis blurs this a bit, but at the cost of a very complicated implementation for doing so.
2. It is much easier to define your own data generation, because you don't have to define simplification rules
   at all.


Will this work with simplifying complex data?
---------------------------------------------

Yes.

Take a look at the `example quality test suite <https://github.com/DRMacIver/conjecture/blob/master/python/tests/test_example_quality.py>`_
for the Python research prototype for some examples of what it can do. The example quality significantly outperforms
that of QuickCheck. 

What are the downsides?
-----------------------

Other than the fact that it is currently very immature and thus still a work in progress, there aren't that many.

The limitations I suspect are intrinsic are:

1. The relation between data generation and shrinking can be a little opaque, and it's not always obvious where
   you should put example markers in order to get good results.
2. The API is pretty intrinsically imperative. In much the same way that Quickcheck doesn't adapt well to
   imperative languages, I don't think this will adapt well to functional ones. There's a reasonably natural
   monadic interface so it shouldn't be *too* bad, but it's probably going to feel a bit alien.
3. Shrinking can be quite slow for very complicated examples when compared to Hypothesis. It usually produces
   something reasonable within the minute timeout though.
4. The data quality is not as good as Hypothesis because it lacks the parametrization feature. I've not yet
   figured out how to fix this. However the data quality is probably about as good as if not better than a
   classic QuickCheck.


References
~~~~~~~~~~

* Property based testing in its modern incarnations almost all are derived from
  `Quickcheck <https://hackage.haskell.org/package/QuickCheck>`_.
* Much of the work that Conjecture is built on comes from advances I made to the core ideas of Quickcheck in
  `Hypothesis <http://hypothesis.readthedocs.org/en/latest/>`_.
* This sort of inversion where you are given a function to call from your tests that controls the testing
  behaviour has been done before in `"eXplode:a Lightweight, General System for Finding Serious Storage
  System Errors" <http://web.stanford.edu/~engler/explode-osdi06.pdf>`_ by Junfeng Yang, Can Sar, and
  Dawson Engler and Stanford. This was designed for deterministically exploring all possible paths and thus
  lacks many of the things that make Conjecture really exciting, but is nevertheless a very similar concept.
* I derived a lot of insights about effectively exploring non-trivial program state using byte streams from the
  `American Fuzzy Lop <http://lcamtuf.coredump.cx/afl/>`_, a security oriented fuzzer.
