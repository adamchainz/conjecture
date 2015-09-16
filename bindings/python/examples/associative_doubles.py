import conjecture as c
import math

def test_associative_doubles(context):
    x = c.declare('x', c.draw_double(context))
    y = c.declare('y', c.draw_double(context))
    z = c.declare('z', c.draw_double(context))
    c.assume(context, not math.isnan(x + y + z));
    assert((x + y) + z == x + (y + z));

if __name__ == '__main__':
    c.TestRunner().run_test(test_associative_doubles)
