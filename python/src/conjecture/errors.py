class ConjectureException(Exception):
    pass


class NoSuchExample(ConjectureException):
    pass


class Frozen(ConjectureException):
    pass


class StopTest(BaseException):
    pass
