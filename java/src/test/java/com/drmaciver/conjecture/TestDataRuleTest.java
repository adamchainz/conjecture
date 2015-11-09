package com.drmaciver.conjecture;

import org.junit.Assert;
import org.junit.Rule;

import java.util.List;

import org.junit.Test;
import org.junit.matchers.JUnitMatchers;

public class TestDataRuleTest {
  private static final DataGenerator<Integer> INTEGER = new IntegerGenerator();
	private static final DataGenerator<List<Integer>> INTEGERS = new ListGenerator<Integer>(INTEGER);
	
	@Rule
	public final TestDataRule data = new TestDataRule();

	@Test
	public void test() {
		int sum = 0;
		for(int t: data.draw(INTEGERS)) sum += t;
		Assert.assertTrue(sum < 100);
	}

	@Test
	public void testAssociatve() {
		int x = data.draw(INTEGER);
		int y = data.draw(INTEGER);
		int z = data.draw(INTEGER);
  
		Assert.assertEquals((x + y) + z, x + (y + z));
	}
}
