package com.drmaciver.conjecture;

public class BytesGenerator implements DataGenerator<byte[]> {
	final int n;

	public BytesGenerator(int n) {
		super();
		this.n = n;
	}

	@Override
	public byte[] draw(TestData data) {
		return data.drawBytes(this.n);
	}

}
