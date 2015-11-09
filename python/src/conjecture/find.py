from conjecture.core import find_interesting_buffer, DEBUG, Settings
from conjecture.errors import NoSuchExample
from conjecture.testdata import TestData


class UnsatisfiedAssumption(Exception):
    pass


def assume(x):
    if not x:
        raise UnsatisfiedAssumption()


def find(draw, check, settings=None):
    seen = False

    def test_function(data):
        nonlocal seen
        value = draw(data)
        try:
            if DEBUG:
                print(data.buffer[:data.index], "->",  value)
            if check(value):
                seen = True
                data.incur_cost(len(repr(data)))
                data.mark_interesting()
            elif not seen and DEBUG:
                print(data.buffer[:data.index], "->",  value)
        except UnsatisfiedAssumption:
            data.mark_invalid()
    buffer = find_interesting_buffer(
        test_function, settings=settings or Settings(
            generations=5000,
        ))
    if buffer is not None:
        result = draw(TestData(buffer))
        assert check(result)
        return result
    raise NoSuchExample()
