#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>
#include<stdio.h>

BlockBuffer::BlockBuffer(int blockNum) {
	this->blockNum = blockNum;
}

BlockBuffer::BlockBuffer(char blocktype) {
	
	//allocate a block on the disk and a buffer in memory to hold the new block of
	//given type using getFreeBlock function and get the return error codes if any
	
	int blockTyp;
	
	if(blocktype == 'R') {
		blockTyp = REC;
	}
	else if(blocktype == 'I') {
		blockTyp = IND_INTERNAL;
	}
	else if(blocktype == 'L') {
		blockTyp = IND_LEAF;
	}
	
	int blockNum = getFreeBlock(blockTyp);
	if(blockNum<0 || blockNum>=DISK_BLOCKS) {
		printf("Error getting free block");
		this->blockNum = blockNum;
		return;
	}
	
	this->blockNum = blockNum;
	
}

// calls the parent class constructor
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

//call parent non-default constructor with 'R' denoting record block
RecBuffer::RecBuffer() : BlockBuffer('R') {}

// call the corresponding parent constructor
IndBuffer::IndBuffer(char blockType) : BlockBuffer(blockType){}

// call the corresponding parent constructor
IndBuffer::IndBuffer(int blockNum) : BlockBuffer(blockNum){}

// call the corresponding parent constructor, 'I' used to denote IndInternal.
IndInternal::IndInternal() : IndBuffer('I'){}

// call the corresponding parent constructor
IndInternal::IndInternal(int blockNum) : IndBuffer(blockNum){}

// this is the way to call parent non-default constructor.'L' used to denote IndLeaf.
IndLeaf::IndLeaf() : IndBuffer('L'){} 

//call parent non-default constructor.
IndLeaf::IndLeaf(int blockNum) : IndBuffer(blockNum){}

int BlockBuffer::getBlockNum() {

	return this->blockNum;

}

// load the block header into the argument pointer
int BlockBuffer::getHeader(struct HeadInfo *head) {
	
	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret!=SUCCESS) {
		return ret;
	}
  	
  	// populate the numEntries, numAttrs and numSlots fields in *head
  	
  	memcpy(&head->numSlots, bufferPtr + 24, 4);
  	memcpy(&head->numAttrs, bufferPtr + 20, 4);
  	memcpy(&head->numEntries, bufferPtr + 16, 4);
  	memcpy(&head->rblock, bufferPtr + 12, 4);
  	memcpy(&head->lblock, bufferPtr + 8, 4);
  	memcpy(&head->pblock, bufferPtr + 4, 4);
  	return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo *head) {

	unsigned char *bufferPtr;
	
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret!=SUCCESS) {
		return ret;
	}
	
	struct HeadInfo *bufferHeader = (struct HeadInfo*)bufferPtr;

    	// copy the fields of the HeadInfo pointed to by head (except reserved) to
    	// the header of the block (pointed to by bufferHeader)
    	
    	bufferHeader->numSlots = head->numSlots;
    	bufferHeader->numAttrs = head->numAttrs;
    	bufferHeader->numEntries = head->numEntries;
    	bufferHeader->rblock = head->rblock;
    	bufferHeader->lblock = head->lblock;
    	bufferHeader->pblock = head->pblock;
    	bufferHeader->blockType = head->blockType;
    	
    	ret = StaticBuffer::setDirtyBit(this->blockNum);
    	
    	if(ret!=SUCCESS) return ret;
    	
    	return SUCCESS;
    	
}

int RecBuffer::getRecord(union Attribute *rec,int slotNum) {

	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret!=SUCCESS) {
		return ret;
	}
	
	struct HeadInfo head;
	this->getHeader(&head);
	
	int attrCount = head.numAttrs;
	int slotCount = head.numSlots;

	int recordSize = attrCount * ATTR_SIZE;
	unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);
	
	memcpy(rec, slotPointer, recordSize);
	
	return SUCCESS;
	
}

int RecBuffer::setRecord(union Attribute* rec, int slotNum) {

	unsigned char *bufferPtr;
	
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret!=SUCCESS) {
		return ret;
	}
	
	struct HeadInfo head;
	this->getHeader(&head);
	
	int attrCount = head.numAttrs;
	int slotCount = head.numSlots;
	
	if(slotNum<0 || slotNum >= slotCount) {
		return E_OUTOFBOUND;
	}
	
	int recordSize = attrCount * ATTR_SIZE;
	unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);
	
	memcpy(slotPointer, rec, recordSize);
	
	ret = StaticBuffer::setDirtyBit(this->blockNum);
	
	if(ret!=SUCCESS) {
		return ret;
	}
	
	return SUCCESS;

}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **bufferPtr) {
	
	int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
	
	if (bufferNum != E_BLOCKNOTINBUFFER) {
	
		for(int bufferIndex=0; bufferIndex<BUFFER_CAPACITY; bufferIndex++) {
			if(bufferIndex == bufferNum) {
				StaticBuffer::metainfo[bufferIndex].timeStamp = 0;
			}
			else {
				StaticBuffer::metainfo[bufferIndex].timeStamp++;
			}
		}
	
	}
	else {
		bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);
		
		if (bufferNum == E_OUTOFBOUND) {
			return E_OUTOFBOUND;
		}
		
		Disk::readBlock(StaticBuffer::blocks[bufferNum],this->blockNum);
		
	}
	
	*bufferPtr = StaticBuffer::blocks[bufferNum];
	
	return SUCCESS;
}

/* used to get the slotmap from a record block
NOTE: this function expects the caller to allocate memory for `*slotMap`
*/
int RecBuffer::getSlotMap(unsigned char *slotMap) {
	unsigned char *bufferPtr;
	
	//get the starting address of the buffer containing the block using loadBlockAndGetBufferPtr
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret!=SUCCESS) {
		return ret;
	}
	
	
	//getting header of the block using getHeader() function
	RecBuffer recordBlock(this->blockNum);
	struct HeadInfo head;
	recordBlock.getHeader(&head);
	
	int slotCount = head.numSlots;
	
	//get a pointer to beginning of slotmap in memory by offsetting HEADER_SIZE
	unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;
	
	//copy values from `slotMapInBuffer` to `slotMap` 
	for(int i=0; i < slotCount; i++) {
		*(slotMap+i) = *(slotMapInBuffer+i);
	}
	
	return SUCCESS; 
}

int RecBuffer::setSlotMap(unsigned char *slotMap) {

	unsigned char *bufferPtr;
	
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	
	if(ret!=SUCCESS) {
		return ret;
	}
	
	//get the header of the block
	HeadInfo thisBlockHeader;
	RecBuffer thisBlock(this->blockNum);
	thisBlock.getHeader(&thisBlockHeader);
	
	int numSlots = thisBlockHeader.numSlots;
	
	//slotMap starts at bufferPtr+HEADER_SIZE. copy contents of slotMap to there
	unsigned char *slotMapInBuffer = bufferPtr+HEADER_SIZE;
	memcpy(slotMapInBuffer, slotMap, numSlots);
	
	//update Dirty bit
	ret = StaticBuffer::setDirtyBit(this->blockNum);
	
	if(ret!=SUCCESS) {
		return ret;
	}
	
	return SUCCESS;

}


int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType) {
	double diff;
	if(attrType == STRING) {
		diff = strcmp(attr1.sVal,attr2.sVal);
	}
	else {
		diff = attr1.nVal - attr2.nVal;
	}
	
	if(diff < 0) return -1;
	if(diff == 0) return 0;
	if(diff > 0) return 1;
	
	return diff;
}

int BlockBuffer::setBlockType(int blockType) {

	unsigned char *bufferPtr;
	
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	
	if(ret!=SUCCESS) return ret;
	
	//store the input block type in the first 4 bytes of the buffer
	// (hint: cast bufferPtr to int32_t* and then assign it)
	int32_t *blockTypePtr = (int32_t*)bufferPtr;
	*blockTypePtr = blockType;
	
	//update the blockAllocMap entry corresponding to the object's block number to `blockType
	StaticBuffer::blockAllocMap[this->blockNum] = blockType;
	
	// update dirty bit by calling StaticBuffer::setDirtyBit()
	ret = StaticBuffer::setDirtyBit(this->blockNum);
	
	if(ret!=SUCCESS) return ret;
	
	return SUCCESS;
	
}

int BlockBuffer::getFreeBlock(int blockType) {

	//iterate through the StaticBuffer::blockAllocMap and find the block number
	int blockNum;
	for(blockNum = 0; blockNum < DISK_BLOCKS; blockNum++) {
		if(StaticBuffer::blockAllocMap[blockNum] == UNUSED_BLK) {
			break;
		}
	}
	
	if(blockNum == DISK_BLOCKS) {
		return E_DISKFULL;
	}
	
	this->blockNum = blockNum;
	
	//find a free buffer using StaticBuffer::getFreeBuffer()
	int bufferIndex = StaticBuffer::getFreeBuffer(blockNum);
	
	// initialize the header of the block passing a struct HeadInfo with values
        // pblock: -1, lblock: -1, rblock: -1, numEntries: 0, numAttrs: 0, numSlots: 0
        // to the setHeader() function.
	
	HeadInfo header;
	
	header.pblock = -1;
	header.lblock = -1;
	header.rblock = -1;
	header.numEntries = 0;
	header.numAttrs = 0;
	header.numSlots = 0;
	
	setHeader(&header);
	

        // update the block type of the block to the input block type using setBlockType().
        setBlockType(blockType);
        
        return blockNum;
        
}

void BlockBuffer::releaseBlock() {
	
	if(this->blockNum == INVALID_BLOCKNUM || StaticBuffer::blockAllocMap[this->blockNum] == UNUSED_BLK)
		return;
	
	//get the buffer num of the buffer assigned to this block
	int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
	
	if(bufferNum != E_BLOCKNOTINBUFFER || bufferNum != E_OUTOFBOUND) {
		StaticBuffer::metainfo[bufferNum].free = true;
	}
	
	StaticBuffer::blockAllocMap[this->blockNum] = UNUSED_BLK;
	
	this->blockNum = INVALID_BLOCKNUM;
	
}

int IndInternal::getEntry(void *ptr, int indexNum) {

	//if indexNum is not in the valid range
	if(indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL) return E_OUTOFBOUND;
	
	unsigned char *bufferPtr;
	// get the starting address of the buffer containing the block using loadBlockAnd
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret != SUCCESS) return ret;
	
	//typecast the void pointer to an internal entry pointer
	struct InternalEntry *internalEntry = (struct InternalEntry *)ptr;
	
	// copy entries from indexNum'th entry to *internalEntry
	unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);
	
	memcpy(&(internalEntry->lChild), entryPtr, sizeof(int32_t));
	memcpy(&(internalEntry->attrVal), entryPtr + 4, sizeof(Attribute));
	memcpy(&(internalEntry->rChild), entryPtr + 20, sizeof(int32_t));
	
	return SUCCESS;
}

int IndLeaf::getEntry(void *ptr, int indexNum) {

	//if indexNum is not in valid range
	if(indexNum < 0 || indexNum >= MAX_KEYS_LEAF) return E_OUTOFBOUND;
	
	unsigned char *bufferPtr;
	//get starting address using loadBlockAndGetBufferPtr
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret != SUCCESS) return ret;
	
	//copy indexNum'th entry using memcpy
	unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
	memcpy((struct Index *)ptr, entryPtr, LEAF_ENTRY_SIZE);
	
	return SUCCESS;
}

int IndInternal::setEntry(void *ptr, int indexNum) {

	if(indexNum < 0 || indexNum >=MAX_KEYS_INTERNAL) return E_OUTOFBOUND;
	
	unsigned char *bufferPtr;
	int ret  = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret!=SUCCESS) return ret;
	
	//typecast void pointer to internal entry pointer
	struct InternalEntry *internalEntry = (struct InternalEntry*)ptr;
	
	unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);
	
	memcpy(entryPtr, &(internalEntry->lChild), sizeof(int32_t));
	memcpy(entryPtr + 4, &(internalEntry->attrVal), ATTR_SIZE);
	memcpy(entryPtr + 20, &(internalEntry->rChild), sizeof(int32_t));
	
	ret = StaticBuffer::setDirtyBit(this->blockNum);
	if(ret!=SUCCESS) return ret;
	
	return SUCCESS;
}

int IndLeaf::setEntry(void *ptr, int indexNum) {

	if(indexNum < 0 || indexNum >= MAX_KEYS_LEAF) return E_OUTOFBOUND;
	
	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	
	if(ret!=SUCCESS) return ret;
	
	unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
	memcpy(entryPtr, (struct Index*)ptr, LEAF_ENTRY_SIZE);
	
	ret = StaticBuffer::setDirtyBit(this->blockNum);
	if(ret!=SUCCESS) return ret;
	
	return SUCCESS;
}
