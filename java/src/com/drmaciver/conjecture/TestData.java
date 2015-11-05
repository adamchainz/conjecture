package com.drmaciver.conjecture;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

public class TestData implements Comparable<TestData>{
	static class Frozen extends RuntimeException {
		/**
		 * A mutation method has been called on a frozen TestData.
		 */
		private static final long serialVersionUID = 1L;

		public Frozen(String arg0) {
			super(arg0);
		}
	}

	static class Interval implements Comparable<Interval> {
		final int start;

		final int end;
		public Interval(int start, int end) {
			super();
			this.start = start;
			this.end = end;
		}

		@Override
		public int compareTo(Interval other) {
			if (this.length() < other.length())
				return -1;
			if (this.length() > other.length())
				return 1;
			if (this.start < other.start)
				return -1;
			if (this.start > other.start)
				return 1;
			return 0;
		}

		@Override
		public boolean equals(Object obj) {
			if (this == obj)
				return true;
			if (obj == null)
				return false;
			if (getClass() != obj.getClass())
				return false;
			Interval other = (Interval) obj;
			if (end != other.end)
				return false;
			if (start != other.start)
				return false;
			return true;
		}

		@Override
		public int hashCode() {
			final int prime = 31;
			int result = 1;
			result = prime * result + end;
			result = prime * result + start;
			return result;
		}

		int length() {
			return this.end - this.start;
		}
	}

	enum Status {
		OVERRUN, INVALID, VALID, INTERESTING;
	}

	static class StopTest extends RuntimeException {
		/**
		 * A condition that ends the current test has occurred.
		 */
		private static final long serialVersionUID = 1L;

	}

	byte[] buffer;
	int checksum;
	private int index = 0;
	private int _cost = 0;
	private Status status = Status.VALID;
	private boolean frozen = false;
	private List<Integer> intervalStarts = new ArrayList<Integer>();
	List<Interval> intervals = new ArrayList<Interval>();
	public TestData(byte[] buffer) {
		this.buffer = buffer.clone();
		this.checksum = Arrays.hashCode(buffer);
	}

	void checkIntegrity(){
		if(Arrays.hashCode(this.buffer) != this.checksum){
			throw new RuntimeException("TestData.buffer has been modified.");
		}
	}
	
	private void assertNotFrozen(String name) {
		if (frozen) {
			throw new Frozen("Cannot call " + name + " on a frozen TestData.");
		}
	}

	public int cost() {
		return _cost;
	}

	public byte[] drawBytes(int n) {
		this.assertNotFrozen("drawBytes");
		this.startExample();
		this.index += n;
		if (this.index > this.buffer.length) {
			this.status = Status.OVERRUN;
			this.freeze();
			throw new StopTest();
		}
		byte[] result = Arrays.copyOfRange(this.buffer, this.index - n, this.index);
		this.stopExample();
		return result;
	}
	
	public byte drawByte(){
		return this.drawBytes(1)[0];
	}

	public void freeze() {
		if (this.frozen)
			return;
		this.frozen = true;
		Collections.sort(this.intervals);
		if (this.status == Status.INTERESTING && this.buffer.length > this.index) {
			this.buffer = Arrays.copyOfRange(this.buffer, 0, this.index);
			this.checksum = Arrays.hashCode(this.buffer);
		}
	}

	public void incurCost(int cost) {
		this.assertNotFrozen("incurCost");
		this._cost += cost;
	}

	public void markInteresting() {
		this.assertNotFrozen("markInteresting");
		if (this.status == Status.VALID) {
			this.status = Status.INTERESTING;
		}
		throw new StopTest();
	}

	public void markInvalid() {
		this.assertNotFrozen("markInvalid");
		if (this.status == Status.VALID) {
			this.status = Status.INVALID;
		}
		throw new StopTest();
	}

	boolean rejected() {
		return this.status == Status.INVALID || this.status == Status.OVERRUN;
	}

	public void startExample() {
		this.assertNotFrozen("startExample");
		this.intervalStarts.add(this.index);
	}

	public void stopExample() {
		this.assertNotFrozen("stopExample");
		int k = this.intervalStarts.remove(this.intervalStarts.size() - 1);
		if (k != this.index) {
			Interval interval = new Interval(k, this.index);
			if (intervals.size() == 0 || intervals.get(intervals.size() - 1) != interval)
				this.intervals.add(interval);
		}
	}

	public Status getStatus() {
		return this.status;
	}

	public int index() {
		return this.index;
	}

	@Override
	public int compareTo(TestData other) {
		if(!(this.frozen && other.frozen)){
			throw new RuntimeException("Cannot compare non frozen TestData");
		}
		if(this.cost() < other.cost()) return -1;
		if(this.cost() > other.cost()) return 1;
		if(this.intervals.size() < other.intervals.size()) return -1;
		if(this.intervals.size() > other.intervals.size()) return 1;
		if(this.buffer.length < other.buffer.length) return -1;
		if(this.buffer.length > other.buffer.length) return 1;
		for(int i = 0; i < this.buffer.length; i++){
			int c = ByteUtils.unsigned(this.buffer[i]);
			int d = ByteUtils.unsigned(other.buffer[i]);
			if(c < d) return -1;
			if(c > d) return 1;
		}
		return 0;
	}
}
