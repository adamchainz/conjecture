package com.drmaciver.conjecture;

import java.util.function.Predicate;

public class Conjecture {
	public static <T> T find(DataGenerator<T> generator, Predicate<T> condition, ConjectureSettings settings) throws NoSuchExample{
		byte[] buffer = TestRunner.findInterestingBuffer((TestData data) ->{
			T value = generator.draw(data);
			if(condition.test(value)){
				// data.incurCost(value.toString().length());
				data.markInteresting();
			}
		}, settings);
		if(buffer == null){
			throw new NoSuchExample();
		} else {
			T result = generator.draw(new TestData(buffer));
			if(!condition.test(result)){
				throw new Flaky("Result did not satisfy condition.");
			}
			return result;
		}
	}
	public static <T> T find(DataGenerator<T> generator, Predicate<T> condition) throws NoSuchExample{
		return Conjecture.find(generator,  condition, null);
	}
}
