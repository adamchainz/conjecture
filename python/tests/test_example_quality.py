from conjecture import find, Settings, NoSuchExample
import conjecture.strategies as st

import math
import pytest
from functools import reduce
import operator


intlist = st.lists(st.n_byte_unsigned(8))


def draw_following(data):
    n = data.draw_bytes(1)[0]
    return data.draw_bytes(n)


def test_a_block_of_bytes_simplifies_to_zeros():
    d = find(lambda d: d.draw_bytes(100), lambda x: True)
    assert d == b'\0' * 100


@pytest.mark.parametrize('n', [0, 10, 100, 200, 501])
def test_a_block_of_bytes_simplifies_to_lexicographically_smallest(n):
    K = 1000
    d = find(
        lambda d: d.draw_bytes(K),
        lambda x: len([c for c in x if c > 0]) >= n
    )
    assert d == b'\0' * (K - n) + b'\1' * n


def test_variable_shrink():
    assert find(draw_following, any) == bytes([1])


@pytest.mark.parametrize('n', list(range(50)) + [100, 1000, 10000, 2 ** 32])
def test_minimum_sum_lists(n):
    x = find(intlist, lambda xs: sum(xs) >= n)
    assert sum(x) == n
    assert 0 not in x


def test_can_find_duplicates():
    x = find(intlist, lambda xs: len(set(xs)) < len(xs))
    assert len(x) == 2
    assert sorted(x) == [0, 0]


def test_minimize_sets_of_sets():
    sos = st.lists(st.lists(st.n_byte_unsigned(8)).map(frozenset)).map(set)

    def union(ls):
        return reduce(operator.or_, ls, frozenset())

    x = find(sos, lambda ls: len(union(ls)) >= 30)
    assert len(union(x)) == 30
    assert sum(map(len, x)) == 30


def length_of_longest_ordered_sequence(xs):
    xs = list(xs)
    if not xs:
        return 0
    # FIXME: Needlessly O(n^2) algorithm, but it's a test so eh.
    lengths = [-1] * len(xs)
    lengths[-1] = 1
    for i in range(len(xs) - 2, -1, -1):
        assert lengths[i] == -1
        for j in range(i + 1, len(xs)):
            assert lengths[j] >= 1
            if xs[j] > xs[i]:
                lengths[i] = max(lengths[i], lengths[j] + 1)
        if lengths[i] < 0:
            lengths[i] = 1
    assert all(t >= 1 for t in lengths)
    return max(lengths)


def test_sorted_integer_subsequences():
    k = 6
    xs = find(
        intlist, lambda t: (
            len(t) <= 30 and length_of_longest_ordered_sequence(t) >= k),
    )
    start = xs[0]
    assert xs == list(range(start, start + k))


def test_reverse_sorted_integer_subsequences():
    k = 6
    xs = find(
        intlist, lambda t: (
            len(t) <= 30 and
            length_of_longest_ordered_sequence(reversed(t)) >= k),
    )
    start = xs[0]
    assert xs == list(range(start, -1, -1))


def test_sorted_list_subsequences():
    k = 6
    xs = find(
        st.lists(intlist), lambda t: (
            len(t) <= 30 and length_of_longest_ordered_sequence(t) >= k),
    )
    assert len(xs) == k
    for i in range(k - 1):
        u = xs[i]
        v = xs[i + 1]
        if len(v) > len(u):
            assert v == u + [0]


def test_reverse_sorted_list_subsequences():
    k = 6
    xs = find(
        st.lists(intlist), lambda t: (
            length_of_longest_ordered_sequence(reversed(t)) >= k),
    )
    assert xs == [[t] for t in range(k - 2, -1, -1)] + [[]]


def test_containment():
    u, v = find(
        st.tuples(intlist, st.n_byte_unsigned(8)),
        lambda uv: uv[1] in uv[0] and uv[1] >= 100
    )
    assert u == [100]
    assert v == 100


def assume(data, x):
    if not x:
        data.mark_invalid()


def test_non_reversible_floats():
    good_list = st.lists(st.floats().filter(math.isfinite))
    find(good_list, lambda xs:  sum(xs) != sum(reversed(xs)))


def test_non_associative_floats():
    tup = st.tuples(
        st.floats(), st.floats(), st.floats()).filter(
        lambda t: all(math.isfinite(s) for s in t))
    find(tup, lambda x: (x[0] + x[1]) + x[2] != x[0] + (x[1] + x[2]))


def mean(ls):
    return sum(ls) / len(ls)


good_list = st.lists(st.floats().filter(math.isfinite)).filter(bool)


def test_out_of_bounds_overflow_mean():
    find(
        good_list, lambda xs: not (min(xs) <= mean(xs) <= max(xs)))


def test_minimal_bool_lists():
    t = find(
        st.lists(st.booleans()), lambda x: any(x) and not all(x)
    )
    assert t == [False, True]


def test_filtering_just_does_not_loop():
    with pytest.raises(NoSuchExample):
        find(
            st.just(False).filter(bool), lambda x: True
        )


def test_can_sort_lists():
    x = find(
        st.lists(st.n_byte_unsigned(8)), lambda x: len(set(x)) >= 10
    )
    assert x == list(range(10))


def test_random_walk():
    find(
        st.lists(st.n_byte_signed(8)).filter(lambda x: len(x) >= 10),
        lambda x: abs(sum(x)) >= 2 ** 64 * math.sqrt(len(x))
    )


def test_minimal_mixed():
    xs = find(
        st.lists(st.booleans() | st.tuples(st.booleans(), st.booleans())),
        lambda x: len(x) >= 10
    )
    assert xs == [False] * 10


def test_minimal_hashing():
    import hashlib
    find(
        lambda d: d.draw_bytes(100),
        lambda s: hashlib.sha1(s).hexdigest()[0] == '0'
    )


def test_constant_lists_through_flatmap():
    x = find(
        st.integer_range(0, 99).flatmap(lambda x: st.lists(st.just(x))),
        lambda x: sum(x) >= 100
    )
    assert sum(x[:-1]) < 100
    assert len(x) * (x[0] - 1) < 100


@st.strategy
def tree(data, height=None):
    if height is None:
        height = st.integer_range(0, 100)(data)
    if height == 0:
        return st.n_byte_unsigned(8)(data)
    heights = st.integer_range(0, height - 1)
    l = heights(data)
    r = heights(data)
    return (tree.base(data, l), tree.base(data, r))


def tree_height(t):
    if isinstance(t, tuple):
        return 1 + max(map(tree_height, t))
    else:
        return 0


def tree_values(t):
    if isinstance(t, tuple):
        return tree_values(t[0]) + tree_values(t[1])
    else:
        return [t]


def test_find_deep_tree():
    d = 15
    xs = find(
        tree(), lambda x: tree_height(x) >= d,
        settings=Settings(
            mutations=100, generations=250,
            buffer_size=16 * 1024,
            max_shrinks=10000
        ))
    assert tree_height(xs) == d
    assert set(tree_values(xs)) == {0}


def test_find_broad_tree():
    xs = find(
        tree(), lambda x: len(tree_values(x)) >= 20,
        settings=Settings(
            mutations=100, generations=250,
            buffer_size=16 * 1024,
            max_shrinks=10000
        ))
    assert tree_values(xs) == [0] * 20


@pytest.mark.parametrize('n', list(range(50)) + [100, 1000, 10000, 2 ** 32])
def test_minimum_sum_tree(n):
    xs = find(
        tree(), lambda x: sum(tree_values(x)) >= n,
        settings=Settings(
            mutations=100, generations=250,
            buffer_size=16 * 1024,
            max_shrinks=10000
        ))
    ts = tree_values(xs)
    assert sum(ts) == n
    if n > 0:
        assert 0 not in ts
