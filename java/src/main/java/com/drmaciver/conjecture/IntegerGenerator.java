package com.drmaciver.conjecture;

public class IntegerGenerator implements DataGenerator<Integer> {
	public Integer doDraw(TestData data) {
		final byte[] bytes = data.drawBytes(4);
		int result = 0;
		for (final byte b : bytes) {
			result *= 256;
			result += ByteUtils.unsigned(b);
		}
		return result;
	}
}