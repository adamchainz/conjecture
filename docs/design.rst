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

Implementation
~~~~~~~~~~~~~~

Note: The current Python research prototype is the best way to understand the hairy details, many of which are
elided here for the sake of brevity. This should be considered a high level overview.

The core idea of Conjecture is very simple: Data generators are build on top of other data generators. Shrinking
happens by shrinking when the functions you call do. There is only one fundamental data generator, which is one
for a block of n bytes. This shrinks lexicographically.

So, if you want to generate an 64-bit integer, you can do that by drawing 8 bytes and interpreting it as a signed
two's complement integer. This would give you the following shrinking rule:

1. Positive is simpler than negative
2. For two integers of the same sign, the one closer to zero is simpler

And shrinking the underlying block of bytes would achieve that.

This works by implementing Conjecture data generators as reading from an underlying stream of bytes, then attempting
to simplify that stream.

However this turns out to be not quite sufficient on its own. There are two problems to overcome:

1. Some shrinks that look good when you're considering the underlying data stream may end up increasing example
   complexity because e.g. they shift the boundary of a block of bytes in a way that causes it to be significantly
   more complicated.
2. There are a very large number of potential shrinks and when the buffer is reasonably large this can make shrinking
   incredibly expensive.

As a result, generators have two additional mechanisms provided to control the shape of shrinking:

1. Tests can note a 'cost'. The intended usage pattern is that this is something like number of bytes that would
   be printed to display the example plus a few additional light hints to shape it. Conjecture will not make
   simplifications that increase the total cost.
2. Tests can mark example boundaries with a start_example/stop_example API. These get converted into a series of
   nested intervals.

Examples can also be rejected, marking them as invalid. This can be used for features like Hypothesis's assume()
function.

Given that API, tests are run against a buffer, and the result is one of four statuses:

1. Overrun: The test tried to read more data from the buffer than was available
2. Invalid: The example was rejected
3. Valid: The example ran successfully without overrunning or being invalid but there's nothing special about it.
4. Interesting: The example was in some way interesting (for tests this means "the test failed").

The goal is to find a buffer that is both interesting and minimal when ordered lexicographically by the following
properties:

1. The total cost of running
2. The number of distinct intervals produced
3. The length of the buffer
4. The buffer itself

(In practice it is not guaranteed to always find the global minima, but it usually finds pretty good local minima)

We first look to find *any* interesting example. This works through multiple iterations of the following process:

First, we generate a random buffer of a fixed configurable size. This is generated uniformly at random.

We now perform N mutations. While the last run's status is overflow, these mutations are designed to reduce bytes (on
the assumption that well designed generators read less data given fewer bytes). Otherwise they primarily include:

1. Rerandomizing sections.
2. Cloning intervals

A mutation is accepted if it doesn't decrease the status of the buffer: So an invalid buffer can't be changed to
an overrun, a valid buffer can't be changed to an invalid buffer. This gives a form of "adaptive assume" where
the fraction of examples tried satisfying assumptions is much higher than it would otherwise be.

After N mutations, if we haven't found an interesting buffer we start again. We run this again for a configurable
number of generations.

If we *have* found an interesting buffer, we now move into shrinking mode, which has a different set of mutations.

All mutations in shrink mode are designed to either decrease the length of a buffer or to move to a lexicographically
smaller buffer of the same size. These are then accepted if they cause a decrease in the example total order (which
they may not do if they result in higher cost or the same cost and more intervals).

Shrinking proceeds as follows:

1. We greedily delete any marked intervals we can from the buffer.
2. We perform a series of 'bytewise' shrinks, where we try to lower the byte at each index to the smallest value
   it can go to. We also perform a bunch of other similar operations such as greedily zeroing larger blocks, reordering
   adjacent pairs, and replacing adjacent pairs of the form (n, 0) with n > 0 with (n - 1, 255). There are a number
   of others.
3. If steps 1 and 2 have produced any changes, start again from 1.
4. Perform a series of 'quadratic' shrinks. These consider every pair of indices i < j into the buffer and attempt
   to perform simultaneous shrinks on each. In particular if buf[i] > buf[j] then it attempts to swap them, and
   if they are both the same value it tries a simultaneous shrink on both of them. 
5. If step 4 performed any changes, then go back to 1, else stop.

If at any point we exceed some maximum configurable number of successful shrinks (2000 by default), we stop. This
helps protect us from the case where the search space is too messy to make good large shrinks so we end up making
an essentially infinite number of tiny ones.

This appears to strike a good balance between performance and quality. In particular it seems to be able to reach
a lot of levels of example quality that are almost impossible with a classic QuickCheck. Two interesting examples
from the example quality tests:

.. code-block:: python

  def test_containment():
      u, v = find(
          st.tuples(intlist, st.n_byte_unsigned(8)),
          lambda uv: uv[1] in uv[0] and uv[1] >= 100
      )
      assert u == [100]
      assert v == 100

This is essentially unachievable to a classic QuickCheck: It wouldn't find any examples at all (it does in
Conjecture because of the cloning in the mutation phase) and if you somehow conspired to let it it wouldn't
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
happen when you don't have to care abotu the structure.`

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


How has nobody thought of this before?
--------------------------------------

I honestly have no idea.

I can point to a number of innovations in Hypothesis that I needed to make in order for it to be obvious to me
that this would work, and anecdotally talking to people it's not a priori obvious to them that the approach
described here is at all possible, but it seems too simple an idea to have been overlooked.

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
