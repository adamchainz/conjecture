===================
What is Conjecture?
===================

The core concept of Conjecture is this: What if you could do the most basic sort of randomized testing which
everybody starts out with, only instead of it being awful it was actually amazing?

In a Conjecture test, you are given a random number generator, you generate some data off it (there are lots of
helper functions to help you generate the sort of data you want), you print some things out to indicate what it
is your program actually did, and then as if by magic all the right random choices are taken to produce as
simple an example as possible.

-------------------
How does that work?
-------------------

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

Once it has found such a sequence it proceeds to attempt to simplify the sequence according to the rules that
one sequence is simpler than another if it is either strictly shorter or the same length but lexicographically
before (i.e. lowering a byte earlier in the sequence will always make the result simpler regardless of what you
do to the rest).

What this means in practice is that we are looking for programs which a) Execute fewer operations and b)
mostly produce smaller bytes from a call to getbytes(n).

The former seems an unambiguous good: If you execute fewer operations and read less data, you're probably doing
something simpler. Why does the latter matter?

Well, it doesn't *intrinsically* matter. But if you can arrange your generators so that they produce simpler
results for lexicographically earlier sequences of bytes then you can arrange your generators so that
simplification of the underlying stream results in simplified values.

For example, when drawing an n-byte integer as long as you do it in big-endian format, simplifying the n bytes
lexicographically corresponds to shrinking the integer according to the ordering that positive is simpler than
negative and otherwise closer to zero is simpler.

Then, when you build other generators on top of other higher level generators, you just need to make sure that
simplification in those generators leads to simplification in yours.

The abstraction is a *bit* leaky in that it is helpful to think in terms of the underlying byte sequence and
how it will get simplified under you, but the worst case scenario of not paying attention to this is that your
examples wont be as simplified as they could be.


------------------------
Why is this a good idea?
------------------------

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
Quickcheck ports.

-------------
Does it work?
-------------

Initial experiments say "Yes, definitely".

The simplifier requires some reasonably careful tuning and to implement some simplifications that you probably
wouldn't bother with in a general binary simplifier: For example, if you have an adjacent pair like (1, 0) you
*do* want to try simplifying to to (0, 1), because that might be the middle of an integer and you need to be
able to shrink it.

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
