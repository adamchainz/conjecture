package com.drmaciver.conjecture;


@FunctionalInterface
public interface DataGenerator<T> {
	public T draw(TestData data);
}
