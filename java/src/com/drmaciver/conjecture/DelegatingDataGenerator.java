package com.drmaciver.conjecture;

public abstract class DelegatingDataGenerator<T> implements DataGenerator<T> {
	DataGenerator<T> underlying = null;

	@Override
	public T draw(TestData data) {
		if(underlying == null){
			underlying = this.computeGenerator();
		}
		return this.underlying.draw(data);
	}

	abstract DataGenerator<T> computeGenerator();

}
