from conjecture.core import find_interesting_buffer, DEBUG, Settings
from conjecture.errors import NoSuchExample
from conjecture.testdata import TestData


def find(draw, check, settings=None):
    seen = False

    def test_function(data):
        nonlocal seen
        value = draw(data)
        if check(value):
            seen = True
            data.incur_cost(len(repr(data)))
            if DEBUG:
                print(data.buffer[:data.index], "->",  value)
            data.mark_interesting()
        elif not seen and DEBUG:
            print(data.buffer[:data.index], "->",  value)
    buffer = find_interesting_buffer(
        test_function, settings=settings or Settings(
            mutations=200, generations=1000,
        ))
    if buffer is not None:
        result = draw(TestData(buffer))
        assert check(result)
        return result
    raise NoSuchExample()
