==========
Conjecture
==========

Conjecture is a new approach to property based testing in C.

The idea is that instead of having distinct concepts of example generation and
tests, everything is a test and everything can generate examples. Testing is a
simple matter of writing functions which take a type of random number generator
but whose output is under Conjecture's control and can be controlled to produce
interesting behaviour and then simplified to produce the minimal possible
behaviour.

The result is a style of testing that is more natural to use, easier to implement,
and yet strictly more powerful than classic Quickcheck style property based testing.

It should also be much easier to write bindings to (although it currently isn't
due to slightly less than well abstracted error handling. This will change)
because it requires very little deep integration with the type system or model
of values in the language. All you need are functions.

------------------
Development Status
------------------

Conjecture is currently not even alpha quality. It is very much a proof of the
concept, and I intend to make it into production ready software, but it's just
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
