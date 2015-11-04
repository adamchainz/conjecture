from enum import IntEnum
from conjecture.errors import Frozen, StopTest


class Status(IntEnum):
    OVERRUN = 0
    INVALID = 1
    VALID = 2
    INTERESTING = 3


class TestData(object):
    def __init__(self, buffer: bytes):
        assert isinstance(buffer, bytes)
        self.buffer = buffer
        self.index = 0
        self.status = Status.VALID
        self.frozen = False
        self.intervals = []
        self.interval_stack = []
        self.cost = 0

    def __assert_not_frozen(self, name):
        if self.frozen:
            raise Frozen(
                "Cannot call %s on frozen TestData" % (
                    name,))

    def start_example(self):
        self.__assert_not_frozen('start_example')
        self.interval_stack.append(self.index)

    def stop_example(self):
        self.__assert_not_frozen('stop_example')
        k = self.interval_stack.pop()
        if k != self.index:
            t = (k, self.index)
            if not self.intervals or self.intervals[-1] != t:
                self.intervals.append(t)

    def incur_cost(self, cost):
        self.__assert_not_frozen('incur_cost')
        assert not self.frozen
        self.cost += cost

    def freeze(self):
        if self.frozen:
            return
        self.frozen = True
        # Intervals are sorted as longest first, then by interval start.
        self.intervals.sort(
            key=lambda se: (se[1] - se[0], se[0])
        )
        if self.status == Status.INTERESTING:
            self.buffer = self.buffer[:self.index]

    def draw_bytes(self, n: int) ->bytes:
        self.__assert_not_frozen('draw_bytes')
        self.index += n
        if self.index > len(self.buffer):
            self.status = Status.OVERRUN
            self.freeze()
            raise StopTest()
        self.intervals.append((self.index - n, self.index))
        result = self.buffer[self.index - n:self.index]
        assert len(result) == n
        return result

    def mark_interesting(self):
        self.__assert_not_frozen('mark_interesting')
        if self.status == Status.VALID:
            self.status = Status.INTERESTING
        raise StopTest()

    def mark_invalid(self):
        self.__assert_not_frozen('mark_invalid')
        if self.status != Status.OVERRUN:
            self.status = Status.INVALID
        raise StopTest()

    @property
    def rejected(self):
        return self.status == Status.INVALID or self.status == Status.OVERRUN
