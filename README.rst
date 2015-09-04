==========
Conjecture
==========

Conjecture is a new approach to property based testing in C.

The core concept of Conjecture is: What if you could do the most basic sort of
randomized testing which everybody starts out with, only instead of it being awful
it was actually amazing?

Classic property based testing has a strict separation between example generation
and testing, which makes it very hard to use for tests that don't easily match that
discipline. Suppose I want to do some calculation and then pick a random element of
the result. How do I do that?

In Conjecture, instead of having distinct concepts of example generation and
tests, everything is a test and everything can generate examples. Testing is a
simple matter of writing functions which take a type of random number generator
but whose output is under Conjecture's control and can be controlled to produce
interesting behaviour and then simplified to produce the minimal possible
behaviour.

The result is a style of testing that is more natural to use, easier to implement,
and yet strictly more powerful than classic Quickcheck style property based testing.

If you want to know more about how it works, there is a `rough design doc also
checked in to this repo <docs/design.rst>`_.

If you want to see some examples of usage, there's `a collection of simple tests
<https://github.com/DRMacIver/conjecture/tree/master/examples>`_ (most or all of
which fail) in the repo demonstrating some common behaviours and approaches.


----------
Community
----------

Conjecture is very young and doesn't really have a community of its own, but
discussion of it is extremely welcome in the `Hypothesis Community <http://hypothesis.readthedocs.org/en/latest/community.html>`_.

In particular we have the IRC channel #hypothesis on Freenode.

Please note that there is `a code of conduct <http://hypothesis.readthedocs.org/en/latest/community.html#code-of-conduct>`_
in effect in this community and you are expected to adhere to it.

------------------
Development Status
------------------

Conjecture is currently not even alpha quality. It is very much a proof of the
concept. I intend to make it into production ready software, but it's just
not there yet. It's not even hanging out in the same time zone.

I'll be working on it a fair bit, but there's quite a lot to do and it's more
likely to make progress if someone is interested in paying for the work.

~~~~~~~~~~~~~~~~~~~
Current limitations
~~~~~~~~~~~~~~~~~~~

1. It is not particularly easy to write bindings which e.g. tie into a
   language's exception system rather than just crashing the process when you
   do something wrong.
2. The quality of data generation is not particularly good - it's essentially
   what you would get out of a normal random number generator, which is not
   really optimal for triggering bugs.
3. Shrinking is currently much slower than it should be.
4. It has not been tested on any significant range of platforms and compilers -
   right now it's very much in a "works on my machine" stage. It is unlikely to
   work on anything that is not 64-bit Linux right now. It definitely will not
   work on Windows and I'm not currently sure how to make it do so.
5. It hasn't really been tested much at all other than manually poking at it
   myself.

---------
Licensing
---------

Conjecture is licensed under the GPL v3. If you wish to use it under a
different license, please contact me at
`licensing@drmaciver.com <mailto:licensing@drmaciver.com>`_.
