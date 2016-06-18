==========
Conjecture
==========

Conjecture is a new approach to property based testing.

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

If you want to see some code there's `a python research prototype demonstrating
the concepts <https://github.com/DRMacIver/conjecture/tree/master/python/>`_.

----------
Community
----------

Conjecture is very young and doesn't really have a community of its own, but
discussion of it is extremely welcome in the `Hypothesis Community <https://hypothesis.readthedocs.io/en/latest/community.html>`_.

In particular we have the IRC channel #hypothesis on Freenode.

Please note that there is `a code of conduct <https://hypothesis.readthedocs.io/en/latest/community.html#code-of-conduct>`_
in effect in this community and you are expected to adhere to it.

------------------
Development Status
------------------

Conjecture is currently a prototype. The python code base demonstrates that it works (extremely well in fact)
but is not ready to use, and besides you should probaby be using `Hypothesis <https://hypothesis.readthedocs.io/en/latest/>`_ instead.

If you're interested in having Conjecture available for your language, I am able and willing to do so for a very
reasonable day rate: The design is one that is extremely easy to port to new languages, so in most cases it is
unlikely to take more than a month of work to get a solid version available.

---------
Licensing
---------

All versions of Conjecture are / will be licensed under the GPL v3. If you wish to use it under a
different license, please contact me at `licensing@drmaciver.com <mailto:licensing@drmaciver.com>`_.
