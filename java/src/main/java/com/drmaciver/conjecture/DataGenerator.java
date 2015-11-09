package com.drmaciver.conjecture;

@FunctionalInterface
public interface DataGenerator<T> {
	public T doDraw(TestData data);
}
