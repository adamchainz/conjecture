package com.drmaciver.conjecture;

import java.util.ArrayList;
import java.util.List;

public class ListGenerator<T> implements DataGenerator<List<T>> {

	private DataGenerator<T> elementGenerator;

	public ListGenerator(DataGenerator<T> elementGenerator){
		this.elementGenerator = elementGenerator;
	}
	
	@Override
	public List<T> draw(TestData data) {
		List<T> result = new ArrayList<T>();
		while(true){
			data.startExample();
			if(data.drawByte() <= 50){
				data.stopExample();
				break;
			}
			result.add(this.elementGenerator.draw(data));
			data.stopExample();
		}
		return result;			
	}
}
