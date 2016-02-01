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

An idea that has been around for a while (it is implemented in QuickCheck, both Erlang and Haskell, test.check
and Hypothesis at the minimum) but was absent from the original QuickCheck is that in many cases you can get
shrinking 'for free' by building your generators on top of existing ones and then just shrinking the inputs.
For example, in Hypothesis if you wanted to produce a generator for sorted lists, you could do:

.. code-block:: python

  def sorted_lists(elements):
      return lists(elements).map(sorted)

This then implements shrinking by shrinking the source list then applying the sort function.

As long as your functions map simpler inputs (according to the shrinking order of the source strategy) to simpler
outputs (according to whatever your preference is), this will produce good results.

The key observation of Conjecture is that there is a single class of base strategies under which you can reasonably
build everything else: Fixed size blocks of bytes, shrinking lexicographically when viewed as a sequence of
unsigned integers in [0, 256).

For example, the following is a good generator for unsigned 64-bit integers:

.. code-block:: python

  def uint64s():
    return byteblock(8).map(lambda b: int.from_bytes(b, 'big'))

If you want *unsigned* integers you can't just use two's complement, you want one's complement: Use the high
bit as the sign bit and leave the remainder alone. This is to get the desired 'positive is simpler than negative,
otherwise closer to zero is simpler' behaviou.

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

When drawing data for Conjecture, we are conceptually drawing from a single large block of bytes (see later for why
this isn't strictly true). Our basic n byte strategy then just reads the next n bytes off the block and updates an
index into it to point to after where it's read. Draws which try to read past the end of the buffer are marked as
invalid.

This reduces the entire process of simplification to one of simplifying a block of bytes. As well as meaning that
we don't have to ever implement our own shrinkers, this turns out to have a number of advantages.

This leads us to the following implementation:


.. code-block:: python

    class Overflow(Exception):
        pass

    class TestData(object):
        def __init__(self, buffer):
            self.buffer = buffer
            self.index = 0

        def draw_bytes(self, n):
            if self.index + n &gt; len(self.buffer):
                raise Overflow()
            result = self.buffer[self.index:self.index+n]
            self.index += n
            return result


      def draw(self, strategy):
          return strategy.do_draw(self)


    class SearchStrategy(object):
          def __init__(self, do_draw=None):
              if do_draw is not None:
                  self.do_draw = do_draw

          def do_draw(self, data):
              raise NotImplementedError()

          def map(self, f):
              return SearchStrategy(lambda data: f(data.draw(self)))

          def flatmap(self, f):
              return SearchStrategy(lambda data: data.draw(f(data.draw(self))))

So the key building blocks in conjecture are the ability to draw from the basic
n-byte blocks and the ability to keep drawing from strategies interactively.

This basic idea works OK, but trying to write good strategies on top of it or a
good simplifier for the data proves challenging. However, a number of further refinements
to the concept make it quite effective. In particular, getting TestData to record a lot of
book-keeping information we can use later (such as the block boundaries) is very helpful.


Getting good data
-----------------

It turns out to be hard to get good data off a pure byte stream. Conjecture does a number of things to make it
easier.


Distribution hints
~~~~~~~~~~~~~~~~~~

A problem we run into is that in this implementation, providing good data and good shrinking can end up in tension.

For example, with a floating point generator, interpreting a block of four bytes as a float produces a very reasonable
shrink (everything is simpler than nan, positive is simpler than negative, smaller is simpler than larger), but a lousy
distribution of data (it is very biased towards the very small and the very large, almost never selects nan or infinity,
and doesn't do a good job of selecting for cases where there are rounding issues). We want at the very least to priortize
a number of special values.

Doing this naively we would implement floats as something like a union of that plus a sample from special values. The problem
is that because the sampling uses fewer bytes than the full float distribution it will always be regarded as "simpler" so you
end up simplifying into bad values.

This causes us to replace our basic strategy with not the n-byte strategy, but the n-byte strategy *with a distribution hint*.

A distribution in this case is a function which takes a random number generator and gives a random n-byte block. This lets you
customize the initial distribution of values while retaining good shrinking.

Caveat: The reason this is a hint is that the data you get back from this strategy does not *necessarily* come from the
distribution. Notably, events that occur with probability 0 in the distribution can occur in the data you get back.

Conceptually we then run the fuzzer in two modes: In the first mode we are building up a buffer by repeatedly calling the
distribution function to get the next bytes, then in the second one if we found an interesting buffer that way we just try
to shrink it using bytewise shrinking on the buffer.

I say "conceptually" because there's actually a third mode.

Mutation
~~~~~~~~

One of the big features that Hypothesis has that the above system lacks is much smarter data generation: Hypothesis is
good at generating correlated data, and has an adaptive assume functionality that allows it to select data that
satisfies a test's assumptions much more often than might be expected.

The exact mechanism that Hypothesis uses is hard to replicate in Conjecture, but what's described in this section is
a sort of analogue. It's not exactly equivalent and does some things better and some things worse than Hypothesis.

The idea is that rather than having two phases, build and shrink, we have three phases: Build, mutate and shrink.

Build is as described above. Mutate works as follows:

1. We have a previous buffer we are mutating from
2. We have a *mutator*, which is a function that takes a previous block of n bytes + a distribution hint for it and
   generates a random new n-byte block.
3. We then run the test similar to build mode, but each time we ask for a new block we get the mutator's block based
   on what we generated last time and what the mutator decides to do.

In the current implementation, this works as follows (but is liable to change details in the near future):

A mutator is a mixture of three single purpose mutators. Each draw randomly chooses one and uses that. Single purpose
mutators include:

1. Use the previous block
2. Use the distribution
3. Draw something lexicographically greater than the previous block
4. Draw something lexicographically smaller than the previous block
5. Draw a duplicate of a block we've already drawn
6. Draw a block that is distinct from all the blocks of this size we've already drawn.

We then require our mutators to *ratchet*. What this means is that we only allow certain transitions, and if a mutator
produces a rejected transition we throw it away and draw a new one.

The way this works is to add the notion of a *state* for a TestData at the end:

1. Overrun: The test tried to read past the end of the buffer.
2. Invalid: The test marked the data as invalid at some point
3. Valid: Nothing to see here.
4. Interesting: This is the sort of thing we want to see 

The idea is that a higher state is always better. A mutation is accepted if the status is at least the previous status,
rejected if it's less. In future there may be some special handling of when it's equal (e.g. using heuristics to show
that even though a test is still over running, it's getting better/worse), but there isn't right now.

We then run mutation as follows:

* We draw a new mutator
* We run it up to ten times.
* If at any point a transition is rejected, we throw away this mutator early.

Starting from a fresh buffer, we run the above in a loop until we have performed 50 mutations. At that point we start
afresh.

If at any point the status strictly increases, we reset the mutation count to zero. If it ever reaches Interesting, we
stop the entire mutation process and proceed to shrinking.


Shrinking data
--------------

The original idea was that we could just shrink the buffer with a relatively standard binary file shrinking
algorithm. This turned out to be mostly impractical.

A number of refinements help a lot.

Automatic pruning
~~~~~~~~~~~~~~~~~

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


Interval marking
~~~~~~~~~~~~~~~~

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

Blockwise shrinking
~~~~~~~~~~~~~~~~~~~

Other than deleting, most shrinking for Conjecture works at the block level: We focus on shrinks that respect
the n-byte blocks that were drawn. It doesn't matter if these end up being violated in the course of the
shrinking, but by starting with them we're able to focus on what's important.

So when running we record where the block boundaries are and their lengths. Then when shrinking we:

1. For each n-byte block, try replacing it with a simpler n-byte block that is also present in the TestData.
2. For each duplicate block, try performing a simultaneous bytewise shrink on every copy of that block (i.e.
   keeping them as duplicates but making them smaller).
3. For each block, perform a bytewise shrink on them individually.

In the end it seems to be better to *not* perform a bytewise shrink on the whole buffer: It causes problems
where by moving data from one block to another you end up with visually worse examples even though it's better
in the underlying ordering. These are then fixed if you let the shrinker run for long enough, but often the
shrinker is run on a timeout.

The bytewise shrink process tries a number of things to keep the block of the same length but lexicographically
earlier.

Interval cloning
~~~~~~~~~~~~~~~~

One of the difficulties causes is when lexicographical reduction can cause you to read less data somewhere
in the middle, and if you then deleted the extra data that wasn't read it could skip on to the next bit and
everything would work.

I don't currently have a very good solution for this, but one thing that seems to help is interval cloning:
Take every pair of intervals where one is strictly shorter than the other and try to replace the longer one
with the shorter one.

This doesn't always work *brilliantly*, and is quadratic in the side of the data, but it seems helpful and
I don't currently have a better solution.


Frequently Asked/Anticipated Questions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

How is this different from Quickcheck style testing?
----------------------------------------------------

It has all of the advantages of Hypothesis, but it's simpler, both from a usage and implementation point of view.

Its main advantages over Hypothesis from a usage point of view are:

1. You can mix test execution and data generation freely. For example, if you perform a calculation in your
   test which returns a list of values and then pick an arbitrary value from that list, that's a random choice
   subject to simplification like any other (it will simplify towards having picked the first element of the
   list). In Hypothesis or Quickcheck there's quite a distinct separation between test execution and example
   generation. Hypothesis blurs this a bit, but at the cost of a very complicated implementation for doing so.
2. It is much easier to define your own data generation, because you don't have to define simplification rules
   at all.
3. It has a built in notion of data size, which helps deal with the problem of accidentally generating too large
   data.
4. Hypothesis has a bit of a "locality" problem - e.g. if you have tuples(integers(), integers()), Hypothesis has
   no ability to tell that the two integers() are the same things, so can't do anything to shrink or test interactions
   between them. Conjecture can easily. An example of this is that Conjecture is able to do something like tuples(
   lists(integers()), integers()).filter(lambda x: x[1] in x[0]) and this works correctly - it can generate large
   examples and it can perform simultaneous shrinking between them.


Will this work with simplifying complex data?
---------------------------------------------

Yes. The branch rewriting Hypothesis on top of this now has better shrinking than Hypothesis does in master.

Will this generate good data?
-----------------------------

It should do. The current data quality is better than Hypothesis in some places, worse than others, and most of the
places where it's worse are I think resolvable.

What are the downsides?
-----------------------

Other than the fact that it is currently very immature and thus still a work in progress, there aren't that many.

The limitations I suspect are intrinsic are:

1. The relation between data generation and shrinking can be a little opaque, and it's not always obvious where
   you should put example markers in order to get good results.
2. The API is pretty intrinsically imperative. In much the same way that Quickcheck doesn't adapt well to
   imperative languages, I don't think this will adapt well to functional ones. There's a reasonably natural
   monadic interface so it shouldn't be *too* bad, but it's probably going to feel a bit alien.
3. You can't actually use it yet, as it's still a research prototype.


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
