from conjecture.bindings import *

def declare(name, value):
    print("%s=%r" % (name, value))
    return value

__all__ = [
    'TestRunner', 'declare'
]

for name in globals():
    if name.startswith('draw_'):
        __all__.append(name)
