package com.drmaciver.conjecture;

public class ConjectureSettings {
	public int getBufferSize() {
		return bufferSize;
	}
	public void setBufferSize(int bufferSize) {
		this.bufferSize = bufferSize;
	}
	public int getMutations() {
		return mutations;
	}
	public void setMutations(int mutations) {
		this.mutations = mutations;
	}
	public int getGenerations() {
		return generations;
	}
	public void setGenerations(int generations) {
		this.generations = generations;
	}
	public int getMaxShrinks() {
		return maxShrinks;
	}
	public void setMaxShrinks(int maxShrinks) {
		this.maxShrinks = maxShrinks;
	}
	public boolean isDebug() {
		return debug;
	}
	public void setDebug(boolean debug) {
		this.debug = debug;
	}
	private int bufferSize = 8 * 1024;
	private int mutations = 50;
	private int generations = 100;
	private int maxShrinks = 2000;
	private boolean debug = false;
	
}
