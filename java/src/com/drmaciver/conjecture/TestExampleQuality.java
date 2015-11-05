package com.drmaciver.conjecture;

import static org.junit.Assert.*;

import java.util.Arrays;
import java.util.List;

import org.junit.Test;

public class TestExampleQuality {

	@Test
	public void testLexicogaphicBytes() throws NoSuchExample {
		byte[] data = Conjecture.find(new BytesGenerator(1000), (t) -> {
			int c = 0;
			for(byte b: t){
				if(b > 0) c++;
			}
			return c >= 100;
		});
		assertEquals(1000, data.length);
		for(int i = 0; i < 1000; i++){
			if(i < 900) assertEquals(0, data[i]);
			else assertEquals(1, data[i]);
		}
	
	}

	static int sum(List<Integer> t){
		int c = 0;
		for(int i: t) c += i;
		return c;
	}
	
	@Test
	public void testSummingIntegers() throws NoSuchExample {
		int n = 1000000;
		ConjectureSettings settings = new ConjectureSettings();
		settings.setDebug(true);
		List<Integer> result = Conjecture.find(new ListGenerator<>(new IntegerGenerator()), (t) -> sum(t) >= n, settings);
		System.out.println(result.toString());
		assertEquals(n, sum(result));
	}
}
