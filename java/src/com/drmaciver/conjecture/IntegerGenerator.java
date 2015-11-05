package com.drmaciver.conjecture;

public class IntegerGenerator implements DataGenerator<Integer> {
	@Override
	public Integer draw(TestData data) {
		byte[] bytes = data.drawBytes(4);
		int result = 0;
		for(byte b: bytes) {
			result *= 256;
			result += ByteUtils.unsigned(b);
		}
		return result;
	}
}