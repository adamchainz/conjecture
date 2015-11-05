package com.drmaciver.conjecture;

import java.util.ArrayList;
import java.util.List;
import java.util.Arrays;
import java.util.Random;
import java.util.function.Consumer;

import com.drmaciver.conjecture.TestData.Interval;
import com.drmaciver.conjecture.TestData.Status;
import com.drmaciver.conjecture.TestData.StopTest;

class TestRunner {
	final Random random;
	Consumer<TestData> _testFunction;
	ConjectureSettings settings;
	int changed = 0;
	int shrinks = 0;
	TestData lastData = null;
	
	public TestRunner(Consumer<TestData> _testFunction, ConjectureSettings settings) {
		super();
		this._testFunction = _testFunction;
		if(settings == null) settings = new ConjectureSettings();
		this.settings = settings;
		this.random = new Random();
	}
	
	private void testFunction(TestData data){
		try{
			this._testFunction.accept(data);
		} catch(StopTest st){
		}
		data.freeze();
	}
	
	void newBuffer(){
		byte[] buffer = new byte[this.settings.getBufferSize()];
		this.random.nextBytes(buffer);
	    TestData data = new TestData(buffer);
	    this.testFunction(data);
	    data.freeze();
	    this.lastData = data;
	}
	
	boolean considerNewTestData(TestData data){
		if(this.lastData.getStatus().compareTo(data.getStatus()) < 0){
			return true;
		}
		if(this.lastData.getStatus().compareTo(data.getStatus()) > 0){
			return false;
		}
		if(data.getStatus() == Status.INVALID){
			return data.index() >= this.lastData.index();
		}
		if(data.getStatus() == Status.OVERRUN){
			return data.index() <= this.lastData.index();
		}
	    if(data.getStatus() == Status.INTERESTING){
	    	return this.lastData.compareTo(data) > 0;
	    }
	    return true;
	}
	
	static class StopShrinking extends RuntimeException{

		/**
		 * 
		 */
		private static final long serialVersionUID = 1L;
		
	}
	
	boolean incorporateNewBuffer(byte[] buffer){
		assert buffer.length <= this.settings.getBufferSize();
		if(Arrays.equals(buffer, this.lastData.buffer)) return false;
		TestData data = new TestData(buffer);
		this.testFunction(data);
		if(this.considerNewTestData(data)){
			if(this.lastData.getStatus() == Status.INTERESTING){
				this.shrinks++;
			}
			this.changed++;
			this.lastData = data;
			if(this.settings.isDebug()) System.out.println(Arrays.toString(data.buffer));
			if(this.shrinks >= this.settings.getMaxShrinks()) throw new StopShrinking();
			return true;
		}
		return false;
	}
	
	void run(){
		try{
			this._run();
		} catch(StopShrinking e){
			
		}
	}

	private void _run() {
		this.newBuffer();
		int mutations = 0;
		int generation = 0;
        while(this.lastData.getStatus() != Status.INTERESTING){
        	if(mutations >= this.settings.getMutations()){
        		generation += 1;
        		if(generation >= this.settings.getGenerations()) return;
        		mutations = 0;
        		this.incorporateNewBuffer(this.mutateDataToNewBuffer());
        	}
        	else {
        		this.newBuffer();
        	}
        }
        // We have successfully found an interesting buffer and shrinking starts here
        
        int changeCounter = -1;
        while(this.changed > changeCounter){
        	changeCounter = this.changed;
        	assert this.lastData.getStatus() == Status.INTERESTING;
      		for(int i = 0; i < this.lastData.intervals.size();){
    			Interval interval = this.lastData.intervals.get(i);
    			if(!this.incorporateNewBuffer(ByteUtils.deleteInterval(this.lastData.buffer, interval.start, interval.end))){
    				i++;
    			}
    		}
      		if(this.changed > changeCounter) continue;
    		for(int i = 0; i < this.lastData.intervals.size(); i++){
    			Interval interval = this.lastData.intervals.get(i);
    			this.incorporateNewBuffer(ByteUtils.zeroInterval(this.lastData.buffer, interval.start, interval.end));        		
    		}
    		for(int i = 0; i < this.lastData.intervals.size(); i++){
    			Interval interval = this.lastData.intervals.get(i);
    			this.incorporateNewBuffer(ByteUtils.sortInterval(this.lastData.buffer, interval.start, interval.end));        		
    		}
      		if(this.changed > changeCounter) continue;
      		
      		for(int i = 0; i < this.lastData.buffer.length - 8; i++){
      			this.incorporateNewBuffer(ByteUtils.zeroInterval(this.lastData.buffer, i, i + 8));
      		}
     		for(int i = 0; i < this.lastData.buffer.length; i++){
     			if(this.lastData.buffer[i] == 0) continue;
     			byte[] buf = this.lastData.buffer.clone();
     			buf[i]--;
     			if(!this.incorporateNewBuffer(buf)) break;
     			for(int c = 0; c < ByteUtils.unsigned(this.lastData.buffer[i]) - 1; c++){
     				buf[i] = (byte)c;
     				if(this.incorporateNewBuffer(buf)) break;
     			}
      		}

     		for(int i = 0; i < this.lastData.buffer.length - 1; i++){
     			this.incorporateNewBuffer(ByteUtils.sortInterval(this.lastData.buffer, i, i + 2));
     		}
     		if(this.changed > changeCounter) continue;
    		for(int i = 0; i < this.lastData.buffer.length; i++){
    			this.incorporateNewBuffer(ByteUtils.deleteInterval(this.lastData.buffer, i, i + 1));
    		}
    		for(int i = 0; i < this.lastData.buffer.length; i++){
    			if(this.lastData.buffer[i] == 0){
    				byte[] buf = this.lastData.buffer.clone();
    				for(int j = i; j >= 0; j--){
    					if(buf[j] != 0){
    						buf[j]--;
    						this.incorporateNewBuffer(buf);
    						break;
    					} else {
    						buf[j] = (byte)255;
    					}
    				}
    			}
    		}
     		if(this.changed > changeCounter) continue;
    		List<List<Integer>> buckets = new ArrayList<>();
    		for(int i = 0; i < 256; i++) buckets.add(new ArrayList<>());
    		for(int i = 0; i < this.lastData.buffer.length; i++){
    			buckets.get(ByteUtils.unsigned(this.lastData.buffer[i])).add(i);
    		}
    		for(List<Integer> bucket: buckets){
    			for(int j: bucket){
    				for(int k: bucket){
    					if(j < k){
    						byte[] buf = this.lastData.buffer.clone();
    						if(buf[j] == buf[k]){
    							if(buf[j] == 0){
    								if(j > 0 && buf[j - 1] != 0 && buf[k - 1] != 0){
    									buf[j - 1]--;
    									buf[k - 1]--;
    									buf[j] = (byte)255;
    									buf[k] = (byte)255;
    									this.incorporateNewBuffer(buf);
    									continue;
    								}
    							} else {
    								for(byte c = 0; c < buf[j]; c++){
    									buf[j] = c;
    									buf[k] = c;
    									if(this.incorporateNewBuffer(buf)) break;
    								}
    							}
    						}
    					}
    				}
    			}
    		}
    		if(this.changed > changeCounter) continue;
    		for(int i = 0; i < this.lastData.buffer.length; i++){
    			if(this.lastData.buffer[i] == 0) continue;
    			for(int j = i + 1; j < this.lastData.buffer.length; j++){
    				if(ByteUtils.unsigned(this.lastData.buffer[i]) > ByteUtils.unsigned(this.lastData.buffer[j])){
    					this.incorporateNewBuffer(ByteUtils.swap(this.lastData.buffer, i, j));
    				}
    				if(this.lastData.buffer[i] != 0 && this.lastData.buffer[j] != 0){
    					byte[] buf = this.lastData.buffer.clone();
    					buf[i]--;
    					buf[j]--;
    					this.incorporateNewBuffer(buf);
    				}
    			}
    		}
        }
	}
	
	private byte[] mutateDataToNewBuffer() {
		int n = Math.min(this.lastData.buffer.length, this.lastData.index());
		if(n == 0){
			return new byte[0];
		}
		if(n == 1){
			byte[] result = new byte[1];
			this.random.nextBytes(result);
			return result;
		}
		byte[] result = this.lastData.buffer.clone();
		if(this.lastData.getStatus() == Status.OVERRUN){
			for(int i = 0; i < result.length; i++){
				if(result[i] == 0) continue;
				switch(this.random.nextInt(3)){
				case 0:
					result[i] = 0;
					break;
				case 1:
					result[i] = (byte) this.random.nextInt(ByteUtils.unsigned(result[i]));
					break;
				case 2:
					continue;
				}
			}
			return result;
		}
		if(this.lastData.intervals.size() <= 1 || this.random.nextInt(3) == 0){
			int u, v;
			if(this.random.nextBoolean() || this.lastData.intervals.size() <= 1){
	            u = this.random.nextInt(this.lastData.buffer.length);
	            v = u + this.random.nextInt(this.lastData.buffer.length - u);
			} else {
				Interval in = this.lastData.intervals.get(this.random.nextInt(this.lastData.intervals.size()));
				u = in.start;
				v = in.end;
			}
			switch(this.random.nextInt(3)){
			case 0: 
				for(int i = u; i < v; i++){
					result[i] = 0;
				}
				break;
			case 1: 
				for(int i = u; i < v; i++){
					result[i] = (byte) 255;
				}
				break;
			case 2:
				for(int i = u; i < v; i++){
					result[i] = (byte) this.random.nextInt(256);
				}
				break;
			}
		} else {
			int i = this.random.nextInt(this.lastData.intervals.size() - 1);			
			int j = i + 1 + this.random.nextInt(this.lastData.intervals.size() - 1 - i);
			Interval int1 = this.lastData.intervals.get(i);
			Interval int2 = this.lastData.intervals.get(j);
			assert int2.length() <= int1.length();
			System.arraycopy(this.lastData.buffer, int2.start, result, int1.start, int2.length());
			if(int1.length() != int2.length()){
				System.arraycopy(this.lastData.buffer, int1.end, result, int1.start + int2.length(), this.lastData.buffer.length - int1.end);
			}			
		}
		return result;
	}
	static byte[] findInterestingBuffer(Consumer<TestData> test, ConjectureSettings settings){
		TestRunner runner = new TestRunner(test, settings);
		runner.run();
		if(runner.lastData.getStatus() == Status.INTERESTING){
			runner.lastData.checkIntegrity();
			System.out.println(Arrays.toString(runner.lastData.buffer));
			return runner.lastData.buffer;
		}
	    return null;
	}
}
