from conjecture.core import find_interesting_buffer, DEBUG, Settings
from conjecture.errors import NoSuchExample
from conjecture.testdata import TestData


def find(draw, check, settings=None):
    def test_function(data):
        value = draw(data)
        if check(value):
            data.incur_cost(len(repr(data)))
            if DEBUG:
                print(data.buffer[:data.index], "->",  value)
            data.mark_interesting()
    buffer = find_interesting_buffer(
        test_function, settings=settings or Settings(
            mutations=50, generations=500,
        ))
    if buffer is not None:
        result = draw(TestData(buffer))
        assert check(result)
        return result
    raise NoSuchExample()
