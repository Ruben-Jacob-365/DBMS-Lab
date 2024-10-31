#include "StaticBuffer.h"
#include<bits/stdc++.h>

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];

StaticBuffer::StaticBuffer() {

	int bmapidx = 0;

	for(int i = 0; i < BLOCK_ALLOCATION_MAP_SIZE; i++) {
		
		unsigned char buffer[BLOCK_SIZE];
		
		Disk::readBlock(buffer,i);
		
		memcpy(blockAllocMap+i*BLOCK_SIZE, buffer, BLOCK_SIZE);
	} 
	
	/*for(int i=0; i<DISK_BLOCKS; i++) {
		std::cout<<(int)blockAllocMap[i]<<" ";
	} */
	
	for(int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
		metainfo[bufferIndex].free = true;
		metainfo[bufferIndex].dirty = false;
		metainfo[bufferIndex].timeStamp = -1;
		metainfo[bufferIndex].blockNum = -1;
	}
}

int StaticBuffer::getFreeBuffer(int blockNum) {
	
	if(blockNum<0 || blockNum > DISK_BLOCKS) {
		return E_OUTOFBOUND;
	} 
	
	//increase timeStamp of all occupied buffers
	for(int bufferIndex = 0; bufferIndex<BUFFER_CAPACITY; bufferIndex++) {
		if(metainfo[bufferIndex].free == false) {
			metainfo[bufferIndex].timeStamp++;
		}
	} 
	
	int bufferNum;
	
	//check for free buffer
	for(bufferNum = 0; bufferNum < BUFFER_CAPACITY; bufferNum++) {
		if(metainfo[bufferNum].free == true)
			break;
	}
	
	//if no buffer is free get buffer with largest timeStamp
	if(bufferNum == BUFFER_CAPACITY) {
		int largestTimeStamp = -1;
		for(int bufferIndex=0; bufferIndex<BUFFER_CAPACITY; bufferIndex++) {
			if(metainfo[bufferIndex].timeStamp > largestTimeStamp) {
				largestTimeStamp = metainfo[bufferIndex].timeStamp;
				bufferNum = bufferIndex;
			}
		}
			
		//if bufferNum is dirty write it back to disk using Disk::writeBlock
		if(metainfo[bufferNum].dirty) {
			Disk::writeBlock(blocks[bufferNum],metainfo[bufferNum].blockNum);
		}
	}
	
	//update metainfo of bufferNum with free:false, dirty:false, blockNum , timeStamp 0
	metainfo[bufferNum].free = false;
	metainfo[bufferNum].dirty = false;
	metainfo[bufferNum].blockNum = blockNum;
	metainfo[bufferNum].timeStamp = 0;
	
	return bufferNum; 
	
}

int StaticBuffer::getBufferNum(int blockNum) {
	if(blockNum<0 || blockNum > DISK_BLOCKS) {
		return E_OUTOFBOUND;
	}
	
	for(int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
		if(metainfo[bufferIndex].blockNum == blockNum) {
			return bufferIndex;
		}
	}
	
	return E_BLOCKNOTINBUFFER;
}

int StaticBuffer::setDirtyBit(int blockNum) {
	
	//find buffer index corresponding to the block using getBufferNum
	int bufferIndex = getBufferNum(blockNum);
	
	if(bufferIndex == E_BLOCKNOTINBUFFER) {
		return E_BLOCKNOTINBUFFER;
	}
	
	if(bufferIndex == E_OUTOFBOUND) {
		return E_OUTOFBOUND;
	}
	
	metainfo[bufferIndex].dirty = true;
	
	return SUCCESS;
	
}

StaticBuffer::~StaticBuffer() {

	int bmapidx=0;
	
	for(int i = 0; i < BLOCK_ALLOCATION_MAP_SIZE; i++) {
		
		unsigned char buffer[BLOCK_SIZE];
		
		memcpy(buffer, blockAllocMap+i*BLOCK_SIZE, BLOCK_SIZE);
		
		Disk::writeBlock(buffer,i);
		
	}
	
	for(int bufferIndex=0; bufferIndex<BUFFER_CAPACITY; bufferIndex++) {
		if(!metainfo[bufferIndex].free && metainfo[bufferIndex].dirty) {
			Disk::writeBlock(blocks[bufferIndex], metainfo[bufferIndex].blockNum);
		}
	}
}
	
int StaticBuffer::getStaticBlockType(int blockNum) {

	if(blockNum < 0 || blockNum >= DISK_BLOCKS) return E_OUTOFBOUND;
	
	int blockType = (int)blockAllocMap[blockNum];
	//printf("%d \n",blockType);
	return blockType;
}

