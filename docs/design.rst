===================
What is Conjecture?
===================

Note: This document is currently ahead of the state of the art in Conjecture itself and is based on some
prototyping I've been doing in a private fork of `Hypothesis <http://hypothesis.readthedocs.org/en/latest/>`_.
The current Conjecture implementation is proof that these ideas work in C, but proof that they can be made to
work *well* is not yet public.

The core concept of Conjecture is this: What if you could do the most basic sort of randomized testing which
everybody starts out with, only instead of it being awful it was actually amazing?

In a Conjecture test, you are given a random number generator, you generate some data off it (there are lots of
helper functions to help you generate the sort of data you want), you print some things out to indicate what it
is your program actually did, and then as if by magic all the right random choices are taken to produce as
simple an example as possible.

How does that work?
~~~~~~~~~~~~~~~~~~~

There is of course no magic. Conjecture runs your test multiple times, suppressing output in all but the last
run, and then once it has found the simplest possible way it can of making your test fail it runs it one final
time but lets it print.

How can it simplify your program?

Internally the random number generator it is feeding you is based off a fixed length sequence of bytes and an
index into the sequence. The basic operation it exposes is to draw the next n bytes starting from the current
index. If this would cause you to draw past the end of the buffer then it aborts the test in a non-fatal way
(i.e. the test stops running immediately, but this is not considered an error).

So first off you simply want to find a sequence of bytes which causes the test to fail when run against it.

Currently this is simply a matter of drawing from /dev/urandom. In future it will become significantly more
complicated. The goal is also to let you be able to use American Fuzzy Lop to feed test programs with it (this
should be very easy to do).

Once it has found such a sequence it tries to find a "simpler" sequence of bytes that will still trigger an
error. One sequence is simpler than another if either it is strictly shorter, or if it is of the same length
but lexicographically before (i.e. on the first byte they differ it has a lower byte).

Thus, once we find a sequence of bytes that causes the test to fail, we try it again with other simpler but
similar sequences of bytes. For example we might trim bytes off the end, or delete sections in the middle, or
we might try zeroing out some bytes, etc. If a simpler sequence also causes the test to fail, we replace our
current best sequence with that one and try to simplify it further.

What this means in practice is that we are looking for programs which a) Execute fewer operations and b)
mostly produce smaller bytes from a call to getbytes(n).

The former seems an unambiguous good: If you execute fewer operations and read less data, you're probably doing
something simpler. Why does the latter matter?

Well, it doesn't *intrinsically* matter. It's a somewhat arbitrary choice, though turns out to be a convenient one
for a variety of reasons. But for any consistent choice of simplification if you can arrange your
generators so that they produce simpler results for simpler sequences of bytes then you can arrange your
generators so that simplification of the underlying stream results in simplified values.

For example, when drawing an n-byte integer as long as you do it in big-endian format, simplifying the n bytes
lexicographically corresponds to shrinking the integer according to the ordering that positive is simpler than
negative and otherwise closer to zero is simpler.

Then, when you build other generators on top of other higher level generators, you just need to make sure that
simplification in those generators leads to simplification in yours.

The abstraction is a *bit* leaky in that it is helpful to think in terms of the underlying byte sequence and
how it will get simplified under you, but the worst case scenario of not paying attention to this is that your
examples wont be as simplified as they could be.

Introspection
-------------

The major problem simplification in Conjecture suffers from is that examples are typically large and will stay
large - a minimal example will typically still be a large buffer, because you need that much buffer to satisfy
the data requirements of the test. The sort of simplification passes you need to do this without understanding
the structure of the data are typically O(n^2), and the size of the underlying buffers for interesting examples
typically has large enough n that this is not viable.

We solve this problem by offering an API to let users mark boundaries in the data stream which correspond to
examples. This consists of a pair of functions conjecture_start_example() and conjecture_end_example(). It is
expected that these will be heavily nested.

These then get turned into intervals, which use to heavily direct the search for simpler examples.
The result is that you have much closer to O(n) intervals, and the boundaries correspond very well to places
that is useful to focus on. This makes the simplification process significantly more tractable.

More on how this works later.

Why is this a good idea?
~~~~~~~~~~~~~~~~~~~~~~~~

It's in many ways the natural evolution of the concepts that come out of property based testing and Quickcheck.

For example, here is a C test that demonstrates that double arithmetic is not associative:


.. code-block:: C

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

It's a bit more verbose than the corresponding property based tests in something like Hypothesis:

.. code-block:: python

    @given(floats(), floats(), floats())
    def test_associative_doubles(x, y, z):
        assume(not math.isnan(x + y + z))
        assert (x + y) + z == x + (y + z)

Much of that verbosity comes from C and could be cleaned up in a more reasonable language. For example,
here's how this might look in Python:

.. code-block:: python

    @runtest
    def test_associative_floats(context):
        x = declare('x', draw_double(context))
        y = declare('y', draw_double(context))
        z = declare('z', draw_double(context))
        assume(not math.isnan(x + y + z))
        assert (x + y) + z == x + (y + z)

Where declare is a simple helper function:

.. code-block:: python

    def declare(name, value):
        print("%s = %r" % (name, value))
        return value


Which lets you readily pare down the excess to the only bit where you genuinely do have to do a little bit of
extra work: Deciding what you want your test to actually output when it runs. But as well as being a problem this is also a benefit. For example there's no difficulty with printing intermediate
values in your test run, because they work the same as generated values.

Why is this better?

The *big* thing that people find frustrating in property based testing is the difficult of chaining together
complex properties. Because example generation and test execution are kept completely separate, it's very hard
to perform additional data generation based on previous results in a way that still simplifies.

In `Hypothesis <http://hypothesis.readthedocs.org/en/latest/>`_ this is managed through a complex system
involving a great deal of mutability (most other Quickchecks don't manage this at all). In Conjecture, because
execution and generation are seamlessly blended in the first place everything just works naturally: It's
perfectly reasonable to write a test that generates some data, does some work, then picks a random example
from the output. This will simplify in exactly the same way that anything else does.

It also makes custom user data generation much easier: Instead of having to compose generators with various
higher order functions (which gets you simplification for free in Hypothesis, test.check and a few others, but
doesn't in most Quickchecks!), you just write a function which takes a Conjecture context object and returns some
data. It can take extra arguments or not if you like. Everything more or less just works out.

Another big advantage is that it is much easier to implement, and the lack of advanced features makes binding to
a C implementation a much more viable option. Simplification is the killer feature of Quickcheck, but it's also
a feature that is hard to do correctly and so most people don't bother. As a result the world is full of bad
Quickcheck ports, and making a version that you can simply bind to instead of writing your own seems like a
worthwhile endeavour.

Does it work?
~~~~~~~~~~~~~

Initial experiments say "Yes, definitely". All the moving parts haven't been demonstrated working together yet,
but I have prototypes of everything that classic Quickcheck does and think I know how to achieve all the things
that Hypothesis does that Quickcheck doesn't.

The simplifier requires some reasonably careful tuning and to implement some simplifications that you probably
wouldn't bother with in a general binary simplifier: For example, if you have an adjacent pair like (1, 0) you
*do* want to try simplifying to to (0, 1), because that might be the middle of an integer and you need to be
able to shrink it. Again, more on this later.

Generators can be a little tricky to write if you want good example output, however experience so far is that
they're still easier to write than for Hypothesis because you don't have to worry so much about simplification.

For example, here is the generator for double precision floating point numbers:


.. code-block:: C

  double conjecture_draw_fractional_double(conjecture_context *context) {
    uint64_t a = conjecture_draw_uint64(context);
    if (a == 0)
      return 0.0;
    uint64_t b = conjecture_draw_uint64_under(context, a);
    return ((double)b) / ((double)a);
  }

  static double nasty_doubles[16] = {
      0.0, 0.5, 1.0 / 3, 10e6, 10e-6, 1.175494351e-38F, 2.2250738585072014e-308,
      1.7976931348623157e+308, 3.402823466e+38, 9007199254740992, 1 - 10e-6,
      1 + 10e-6, 1.192092896e-07, 2.2204460492503131e-016, INFINITY, NAN};

  double conjecture_draw_double(conjecture_context *context) {
    // Start from the other end so that shrinking puts us out of the nasty zone
    uint8_t branch = 255 - conjecture_draw_uint8(context);
    if (branch < 32) {
      double base = nasty_doubles[branch & 15];
      if (branch & 16) {
        base = -base;
      }
      return base;
    } else {
      int64_t integral_part = conjecture_draw_int64(context);
      double fractional_part = conjecture_draw_fractional_double(context);
      return (double)integral_part + fractional_part;
    }
  }

It's not particularly user friendly, but you should see the Hypothesis one...

In particular, floating point simplification for Hypothesis was a complete pain to write and has never really
worked very well, whileas in this case by picking some good primitives to build off we've got something that
works more or less out of the box with really not very much effort.

In general Hypothesis has done a pretty good job of demonstrating the thesis that designing generators so that
they simplify well when you pass in simplified arguments is an effective strategy, and the cases where I was
worried that Conjecture would not simplify well do not appear to be major problems.

It is possible that Conjecture will turn out to provide less effective simplification than Quickcheck, but I
think that it's already demonstrated that it produces simplification that is good enough that any shortfall is
more than made up for by its benefits, and I actually think it's possible that Conjecture's approach will prove
better over all because it's more able to escape local minima.

Design of the simplifier
~~~~~~~~~~~~~~~~~~~~~~~~

Note: This simplifier currently only exists in a separate Python prototype, and is very much a work in progress
which is liable to change. It is pretty good, but still needs further development.

Further note: A lot of the specific details of this are a bit ad hoc and are chosen simply to make a class of
examples work well. The general theme is likely to remain, but specific details and magic numbers will almost
certainly change.

The goal of the simplifier is to do a length and then lexicographic minimization over buffers which produce a
test failure. It isn't completely blind - it looks at the structure of the examples being read as hinted at by
the API and uses this to guide the search.

The simplifier always maintains a "current best" buffer. We only ever consider buffers that are simpler than it,
so whenever we find a new example that falsifies the test we always replace the current buffer. When we do this,
we also automatically trim the buffer to contain only the initial prefix of it that was actually read: Because
there is no way to inspect what's coming next this is always valid. This allows many simplifications which
ostensibly have nothing to do with the size of the buffer to shrink it anyway, and helps escape a number of
cases where we could potentially accidentally make the example more complicated again because we didn't notice
a shrink in time.

For our current buffer, we also have a deduplicated list of intervals stored as half-open [start, end). These
are sorted from most complex to least complex, where one interval is more complex than another if it is longer 
or if they are the same length but the corresponding bytes in the buffer sort lexicographically after it (i.e.
the same order we consider buffers in).

Simplification proceeds in two passes:

The first pass is responsible for just deleting as much data as it feasibly can, getting the buffer small enough
that we can perform more intensive simplifications.

This works by greedily deleting intervals, starting from the most complex and proceeding to the least. We skip
all intervals that are really small because experience suggests that there are a lot of small intervals and
they're not worth the cost of trying to delete.

A detail: Rather than proceeding from the most to the least complex every time, we actually maintain an index
and always shrink the i'th most complicated interval, incrementing i each time. When we hit the end of the
list of intervals, if we've made any changes since the start we begin again from the beginning, else we stop.

The reason for this is that starting from the beginning each time means that we will be repeatedly trying the
same deletions over and over again, which will usually not work. Doing it this way means that we will be less
likely to try useless deletions but will still have the property that the final result is a local minimum for
this operation.

The following is more or less the the current Python prototype of this:


.. code-block:: python

    def delete_intervals(self):
        changed = True
        while changed:
            changed = False
            i = 0
            while i < len(self.intervals):
                it = self.intervals[i]
                if it[1] <= it[0] + 8:
                    break
                buf = self.current_best
                changed |= check(buf[:it[0]] + buf[it[1]:])
                i += 1

The method self.check returns whether the buffer it is passed falsifies the test and if it does updates the
current buffer and its corresponding intervals.

Once this is done (which typically results in very large buffer reductions - in my test cases I was often seeing
an order of magnitude or more) we proceed to try to simplify values.

We do this by considering each interval in turn, using the same iteration strategy as above but proceeding
instead from *least* complicated to most complicated.

When simplifying an interval we first try to delete it. This usually doesn't work, but it works just often
enough to be worth trying and saving us the later work. If that does succeed we stop there.

Otherwise we then try to replace it with a lexicographically minimal sequence of bytes of the same length. We
do this by iterating the following operations to a fixed point:

1. Try 'capping' the buffer in the interval, by replacing each byte with min(c, b) for some c < 256.
2. For each byte, starting from left to right, try replacing it with a strictly smaller byte. A linear probe up
   to the value of the byte works pretty well here, though a more complicated solution seems to work slightly
   better while still retaining most of the benefits.
3. Try replacing the sequence with a lexicographically earlier sequence chosen uniformly at random. Try this
   up to 5 times until one of them succeeds.
4. If the random probes didnt work, try replacing the buffer with its immediate lexicographic predecessor (this
   is the same as subtracting one from it interpreted as an unsigned n-byte big-endian integer).

Note that we only run each of 2 and 3 *once* before circling back to the beginning. This is important, because
neither of them are very aggressive simplifications and will frequently significantly increase a byte in a later
element of the sequence. This then opens up the possibility of further simplification to occur in 1 which *is*
a much more aggressive simplifier.

The choice of smaller bytes we use is:

.. code-block:: python

    def up_to(n):
        if n <= 0:
            return
        if n <= 1:
            yield 0
            return
        possibilities = {0, 1}
        for i in hrange(8):
            k = 2 ** i
            if k > n:
                break
            possibilities.add(n & ~k)
            possibilities.add(n >> i)
        possibilities.add(n - 1)
        possibilities.discard(n)
        for k in sorted(possibilities):
            assert k < n
            yield k

This seems to give a good selection of smaller numbers while bounding the number of bytes we try (it gives < 20
numbers while a linear probe might give up to 255).

We also use this for the cap by trying caps which are up_to(maximum).

Once we have the buffer minimized according to these (even if this resulted in no changes) we try *cloning* it.

The way this works is that we find all intervals which are more complicated than the current interval and try
replacing the sequence there with the sequence at the current interval.

Note: It is very rare that it works to try to replace a strictly longer interval, but it works just often enough
and provides a substantial enough benefit when it does that it's not worth limiting the size of the target.

We do this in much the same way as the above, always using the current sequence of intervals and looking at the
i'th one, looping around to the beginning if we successfully changed anything.

Note that it doesn't matter if the current intervals overlap with this one. In particular it's entirely possible
to replace an interval that contains the current one.

The reason for the cloning operation is twofold:

1. It gives access to optimisations that might be much harder to reach.
2. It saves a lot of work. Rather than running essentially duplicate shrinking operations, by repeatedly cloning
   an interval once it has been shrunk we don't need to spend so much time shrinking the interval it replaces
   when we get to it.

Additional work is definitely needed on this, but for most examples I've tried the algorithm described produces
results that are comparably good to Hypothesis.

How do I use it?
~~~~~~~~~~~~~~~~

Right now Conjecture is implemented as a C library (bindings are totally possible and will be coming) and you
can check out `some usage examples in the git repo <https://github.com/DRMacIver/conjecture/tree/master/examples>`_.

This section is more intended to be a high level description of how to write tests and generators with it.

Writing tests
--------------

Writing tests is easy: You write a function that takes a conjecture_context and some optional payload data,
you call some data generation functions (either your own or Conjecture provided ones) using that context, you
print some output to give you the information you want out of your test (e.g. what values are generated) and then
you let conjecture run it.

Writing data generators
------------------------

Writing data generators is relatively easy but requires a little bit of care if you want to get good examples
with simplification.

The core idea is that you simply write data generators by calling other data generators and building on the
results. Everything else should work out for you.

However there are some useful principles to bear in mind that will cause things to work out *better* for you.

Simpler inputs lead to simpler outputs
--------------------------------------


Here is the code the current prototype uses to generate an unsigned 64 bit integer:

.. code-block:: C

    uint64_t conjecture_draw_uint64(conjecture_context *context) {
      unsigned char length = conjecture_draw_uint8(context) & 7;
      unsigned char buffer[8];
      conjecture_draw_bytes(context, 8, buffer);
      uint64_t result = 0;
      for(int i = 0; i <= length; i++) {
        result = (result << 8) + (uint64_t)buffer[i];
      }
      return result;
    }

It reads 8 bytes for the integer off in big endian format. Why big endian? Because "simplicity" for a getbytes
call is specifically defined in lexicographic order: One block of n bytes is simpler than another block of n
bytes if it is smaller (interpreted as an unsigned integer between 0 and 255) in the first byte they differ.

This corresponds precisely to the order of the blocks as big-endian integers: Reducing a high byte always
reduces the integer more than reducing a low byte. If we'd instead read the integer off in little endian order
then 256 would be simpler than 1 because the byte at which they differ comes later.

Simplifying earlier generators may change later generators
----------------------------------------------------------

There's something that is both a feature and a bug about simplifying the underlying data stream: It creates
unintentional dependencies between data.

For example, suppose I have the following code from one of the examples above:

.. code-block:: C

      double x = conjecture_draw_double(context);
      double y = conjecture_draw_double(context);

Simplifying doubles will push them towards examples that are closer to small integers.

However, simplifying x may completely change the values drawn for y! It might become simpler, it might become
more complicated. There's no way to predict. It's essentially an entirely fresh draw.
 
The reason is that there are no "boundaries" in the underlying byte stream, and generators may consume a
variable number of bytes. So if simplifying x changes the number of bytes the generator consumes, it will
result in y starting from a completely different index into the data stream than it did before and thus getting
a different result.

We're never actually "undoing" progress, because progress is happening on the underlying
data stream, but it can seem that we may locally move from simpler examples to more complicated ones.

This is both good and bad. It's bad because it may block some simplifications - a simplification of x may be
valid but unusable because it would cause y to change to something that no longer triggers the problem. It's
good because it may enable simplifications - classically simplification can tend to get stuck in local minima,
and allowing it to sometimes increase perceived complexity can actually help produce better end results.

On balance it seems more bad than good, but it doesn't seem to be a major problem in practice.

The key to avoiding it seems to be that as your generator is simplified it should become "stable" in the number
of bytes it consumes from the underlying data. For example, here's the generator for drawing a bounded uint64_t:

.. code-block:: C


    uint64_t conjecture_draw_uint64_under(conjecture_context *context,
                                          uint64_t max_value) {
      if(max_value == 0) {
        return 0;
      }
      uint64_t mask = saturate(max_value);
      while(true) {
        uint64_t probe = mask & conjecture_draw_uint64(context);
        if(probe <= max_value) {
          return probe;
        }
      }
    }

This consumes a variable number of bytes, but if you simplify the high most bytes down it rapidly converges
to only consuming a fixed number (8) of bytes. From that point on, simplification of the value becomes stable
and won't change subsequent calls.

Try to make calls deletable
----------------------------

Often generators which call variable numbers of other generators will do so in some predictable pattern. e.g.
through a repeated call to some other generator.

If possible, you should try to make it so that if some of these calls are replaced with values to later calls,
this would stop the process early.

For example, here is the Conjecture code for generating a null terminated string:


.. code-block:: C

    char *conjecture_draw_string(conjecture_context *context) {
      size_t max_length = (size_t)conjecture_draw_small_uint64(context);
      char *data = malloc(max_length + 1);
      for(size_t i = 0; i < max_length; i++) {
        unsigned char c;
        conjecture_draw_bytes(context, 1, &c);
        data[i] = c;
        if(c == 0)
          return data;
      }
      data[max_length] = 0;
      return data;
    }

We do decide on the length up front, but we also have the option of stopping early: If any point we happen
to generate a null character, we stop right there and then. This means that if a chunk of the draws in the middle
were deleted, we would just generate a shorter string.

This can be somewhat in tension with the previous heuristic, but in practice it actually often isn't: e.g. deleting
a chunk of characters in the middle of the string actually leaves the bytes read after the string perfectly
preserved. If you lower a byte in the string to zero it *will* change the subsequent calls, but because deletion
is tried first this will usually stabilize pretty reasonably.


Details of the C implementation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These will almost certainly change massively as the code evolves and I start trying to support platforms that
aren't my development laptop, but here's how it currently works:

All tests are written under the assumption that they may crash the process, so the way to report errors is to
just do anything that will cause you to exit with a non-zero status code. e.g. assertions work perfectly fine,
either in test code or outside of it. Notably, something that causes a premature exit with a status code of zero
is *not* considered a test failure. It could easily be made to be if neccessary but I don't currently have a good
argument in favour of it doing so.

In aid of this, running a test forks, immediately redirects stdout and stdin to /dev/null, then executes the
test function.

A very small shared memory segment is maintained for communicating with the parent process. Currently it just
contains a single boolean flag that indicates whether an example was rejected (which usually means it tried to
read past the end of the buffer, although you can also explicitly reject an example).

This allows us to safely test whether a buffer should cause a failure without worrying about crashing or
corrupting the controlling process. Then if and when we *do* find a failure, the final step is that we run the
failing test case in the controlling process. This *should* crash the process. If it does not, we complain about
the test being flaky and crash the process anyway.

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


Why abort the test when you read past the end of the buffer?
------------------------------------------------------------

In principle you could just generate more random data when you reach the end. Why not do that?

The answer is mostly "It's simpler this way". Conjecture is designed to run its tests in a forked subprocess,
and it's easier and more resilient to just grow the buffer to the size needed in the controlling process and
pass it in than to pass a lot of data back to the parent process.

Moreover, the feature of stopping when you hit a certain number of bytes read *is* essential. This doesn't stop
you growing the buffer, but it does mean you're going to at some point need to do this anyway.

There are two major reasons to do this:

1. It naturally provides a way of bounding you away from the case where you accidentally generate massive
   examples. This can easily happen by accident when just generating things at random, and having a cap on the
   number of bytes read will (for most sensible usage patterns) intrinsically prevent that and cause you to
   sample from the conditional distribution of things that are not ridiculously large.
2. When simplifying, once you have an example where you have n bytes, as soon as you try to read the n + 1th
   byte you're definitely not considering a simpler example and thus should discard this immediately rather than
   wasting time trying to find out whether or not it's a failing example.

Will this work with simplifying complex data?
---------------------------------------------

Yes.

I have a prototype based on these concepts which passes most of alightly modified version of Hypothesis's example
shrinking. It takes 2 or 3 times as long in some of these tests, but this is competing against a heavily optimised
implementation which frequently takes a tenth of the time of Haskell Quickcheck.

What are the downsides?
-----------------------

Other than the fact that it is currently very immature and thus still a work in progress, there aren't that many.

The limitations I suspect are intrinsic are:

1. The relation between data generation and shrinking can be a little opaque, and it's not always obvious where
   you should put example markers in order to get good results.
2. The API is pretty intrinsically imperative. In much the same way that Quickcheck doesn't adapt well to
   imperative languages, I don't think this will adapt well to functional ones. There's a reasonably natural
   monadic interface so it shouldn't be *too* bad, but it's probably going to feel a bit alien.

Current limitations that I think I know how to solve:

1. Right now I'm not completely clear on how to get great quality examples out of it - the distribution is a bit
   too 'flat' and lacks an equivalent to `Hypothesis's parametrization <http://hypothesis.readthedocs.org/en/latest/internals.html#parametrization>`_.
   It should be possible to fix this by making generation of the byte stream itself smarter, e.g. by generating an
   example which passes the assumptions of the test and then trying a sequence of mutations on it.
2. assume() in conjecture is not adaptive like in Hypothesis. It just aborts the whole test. I think I can
   actually make assume smarter than in Hypothesis by selective editing of the data stream, but it's hard to
   say for sure.
   
But both of these are "merely Quickcheck level good" which is a nice problem to have.

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
