#include "BPlusTree.h"
#include <bits/stdc++.h>
#include <cstring>

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op) {

	//declare searchIndex which will be used to store search index for attrName
	IndexId searchIndex;

	
	//get search index corresponding to attrName
	AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);
	
	AttrCatEntry attrCatEntry;
	AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
	
	//declare block and index which will be used during search
	int block = -1,index = -1;
	
	if(searchIndex.block == -1 && searchIndex.index == -1) {
		//search is done for the first time, start from root
		
		block = attrCatEntry.rootBlock;
		index = 0;
		
		if(block == -1) {
			return RecId{-1, -1};
		}		
	} else {
		//a valid searchIndex points to an entry in the leaf index of the attribute's
		//B+ tree which had previously satisfied op
		
		block = searchIndex.block;
		index = searchIndex.index + 1;
		
		// load block into leaf using IndLeaf::IndLeaf()
		IndLeaf leaf(block);
		
		//get header of leaf
		HeadInfo leafHead;
		leaf.getHeader(&leafHead);
		
		if(index >= leafHead.numEntries) {
			// this block finished start from next block
			block = leafHead.rblock;
			index = 0;
			
			if(block == -1) {
				//end of linked list
				return RecId{-1, -1};
			}
		}
	}
	
	/****** Traverse through all the internal nodes according to value of attrVal and op ***/
	
	//only needed when search restarts from root block and root is not leaf
	while(StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) {
	
		//load block into internalBlk using IndInternal
		IndInternal internalBlk(block);
		
		//getHeader
		HeadInfo intHead;
		internalBlk.getHeader(&intHead);
		
		InternalEntry intEntry;
		
		if(op == NE || op == LT || op == LE) {
			//we have to move left in these cases
			
			//load entry of first slot of the block into intEntry
			internalBlk.getEntry(&intEntry, 0);
			
			block = intEntry.lChild;
		} else {
			// move to the left child of the first entry that is >= attrVal
			// (first  entry that matched could be in left)
			
			//traverse through all entries of internalBlk and find satisfying entry
			int intIndex = 0;
			
			while(intIndex < intHead.numEntries) {
				
				internalBlk.getEntry(&intEntry, intIndex);
				
				int cmpVal = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);
				
				if( 
				    (op == EQ && cmpVal >= 0) ||
				    (op == GT && cmpVal >0) ||
				    (op == GE && cmpVal >=0)
				  ) {
				  	break;
				  }
				  
				  intIndex++;
			}
			
			if(intIndex < intHead.numEntries) {
				//move to the left child of that entry
				block = intEntry.lChild;
			}
			else {
				//intEntry holds last entry and it has val < attrVal
				block = intEntry.rChild;
			}
		}
	}
	
	//now block has block num of a leaf index block
	
	/***** identify the first leaf index entry from the current position that satisfies our condition   ******/
	while(block != -1) {
		//load block into leafBlk 
		IndLeaf leafBlk(block);
		
		//get Header		
		HeadInfo leafHead;
		leafBlk.getHeader(&leafHead);
		
		//declare leafEntry
		
		Index leafEntry;
		
		while(index < leafHead.numEntries) {
			//load entry corresponding to block and index into leafEntry
			leafBlk.getEntry(&leafEntry, index);
			
			int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType);
			
			if(
			   (op == EQ && cmpVal == 0) ||
			   (op == LE && cmpVal <= 0) ||
			   (op == LT && cmpVal < 0) ||
			   (op == GT && cmpVal > 0) ||
			   (op == GE && cmpVal >= 0) ||
			   (op == NE && cmpVal != 0) 
			   ) {
			   	searchIndex.block = block;
			   	searchIndex.index = index;
			   	
			   	AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);
			   	
			   	RecId recId = {leafEntry.block, leafEntry.slot};
			   	
			   	return recId;
			   }
			   else if ((op == EQ || op == LE || op == LT) && cmpVal > 0) {
			   	//future entries will not be satisfied since entries are sorted
			   	
			   	return RecId{-1, -1};
			   }
			   
			   index++;
		}
		
		//only for NE we have to check entire linked list;
		//for all other op it is guaranteed that block being searched will have an entry satisfying op
		if(op != NE) break;
		
		block = leafHead.rblock;
		index = 0;
	}	
	
	return RecId{-1, -1};
}

int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) {

	if(relId == RELCAT_RELID || relId == ATTRCAT_RELID) return E_NOTPERMITTED;
	
	//get attribute cat entry of attrname from attr Cache
	AttrCatEntry attrCatEntry;
	int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
	if(ret!= SUCCESS) return ret;
	
	//if index already exists
	if(attrCatEntry.rootBlock != -1) {
		return SUCCESS;
	}
	
	/***** Creating a new B+ tree *****/
	
	//get a free leaf block using IndLeaf constructor
	IndLeaf rootBlockBuf;
	
	int rootBlock = rootBlockBuf.getBlockNum();
	// if there is no more disk space for creating an index
	if(rootBlock == E_DISKFULL) return E_DISKFULL;
	
	attrCatEntry.rootBlock = rootBlock;
	AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
	RelCatEntry relCatEntry;
	
	//load relation catalog entry using getRelCatEntry
	RelCacheTable::getRelCatEntry(relId, &relCatEntry);
	
	int block = relCatEntry.firstBlk;
	
	/***** traverse all blocks in the relation and insert them one by one into the B+ tree*/
	
	while(block != -1) {
		RecBuffer currBlock(block);
		
		//get slotMap
		unsigned char slotMap[relCatEntry.numSlotsPerBlk];
		currBlock.getSlotMap(slotMap);
		
		for(int slot = 0; slot < relCatEntry.numSlotsPerBlk; slot++) {
		
			if(slotMap[slot] == SLOT_OCCUPIED) {
			
				//load the record corresponding to the slot into 'record'
				Attribute record[relCatEntry.numAttrs];
				currBlock.getRecord(record, slot);
				
				//declare recId to store this rec-id
				RecId recId = {block, slot};
				
				//insert the attribute value corresponding to attrName into tree
				Attribute attrVal = record[attrCatEntry.offset];
				int ret = bPlusInsert(relId, attrName, attrVal, recId);
				
				if(ret == E_DISKFULL) return E_DISKFULL;
			}
		}
		//get header to go to next block
		HeadInfo header;
		currBlock.getHeader(&header);
		
		block = header.rblock;
	}
	
	return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum) {

	if(rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS) return E_OUTOFBOUND;
	
	int type = StaticBuffer::getStaticBlockType(rootBlockNum);
	
	if(type == IND_LEAF) {
	
		IndLeaf leafBlk(rootBlockNum);
		leafBlk.releaseBlock();
		return SUCCESS;
		
	} else if(type == IND_INTERNAL) {
	
		IndInternal internalBlk(rootBlockNum);
		//get header
		HeadInfo header;
		internalBlk.getHeader(&header);
		
		//iterate through all entries of internalBlk, destroy lchild of first and rchild of all entries
		InternalEntry internalEntry;
		internalBlk.getEntry(&internalEntry, 0);
		BPlusTree::bPlusDestroy(internalEntry.lChild);
		
		for(int index = 0; index < header.numEntries; index++){
			internalBlk.getEntry(&internalEntry, index);
			BPlusTree::bPlusDestroy(internalEntry.rChild);
		}
		
		//release the internal block
		internalBlk.releaseBlock();
		
		return SUCCESS;
				
	} else {
		return E_INVALIDBLOCK;
	}
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId){

	//get attr cat Entry
	AttrCatEntry attrCatEntry;
	int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
	
	if(ret!=SUCCESS) return ret;
	
	int blockNum = attrCatEntry.rootBlock;
	
	if(blockNum == -1) 
		return E_NOINDEX;
	
	//find leaf to insert 
	int leafBlkNum = BPlusTree::findLeafToInsert(blockNum, attrVal, attrCatEntry.attrType);
	
	//insert attrval and recId into leaf block at blockNum
	Index entry;
	entry.attrVal = attrVal;
	entry.block = recId.block;
	entry.slot = recId.slot;
	ret = BPlusTree::insertIntoLeaf(relId, attrName, leafBlkNum, entry);
	if(ret == E_DISKFULL) {
		//destroy existing B+ tree
		BPlusTree::bPlusDestroy(leafBlkNum);
		
		//update rootBlock of attribute to -1
		attrCatEntry.rootBlock = -1;
		AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
		
		return E_DISKFULL;
	}
	
	return SUCCESS;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType) {

	int blockNum = rootBlock;
	while(StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF) {
	
		IndInternal internalBlk(blockNum);
		//get header;
		HeadInfo internalHeader;
		internalBlk.getHeader(&internalHeader);
		
		//iterate through all entries to find first entry with attrVal >= value to insert
		int index;
		InternalEntry intEntry;
		for(index = 0; index < internalHeader.numEntries; index++) {
			
			internalBlk.getEntry(&intEntry, index);
			int cmpval = compareAttrs(intEntry.attrVal, attrVal, attrType);
			
			if(cmpval >= 0) break;
		}
		
		if(index >= internalHeader.numEntries) { //no such entry is found
			// set blockNum = rchild of rightmost entry
			blockNum = intEntry.rChild;
		} else {
			blockNum = intEntry.lChild;
		}
	}
	
	return blockNum;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry) {

	//get the attribute cache entry corresponding to attrName
	AttrCatEntry attrCatEntry;
	AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
	
	IndLeaf leafBlk(blockNum);
	
	//get header 
	HeadInfo blockHeader;
	leafBlk.getHeader(&blockHeader);
	
	//the following varaible will be used to store a list of index entries with existing indices + the new index to insert
	Index indices[blockHeader.numEntries + 1];
	
	/* Iterate through all the entries in the block and copy them to the array indices
	Also insert `indexEntry` at appropriate position in the indices array maintaining
	the ascending order
	- use IndLeaf::getEntry() to get the entry
	- use compareAttrs() declared in BlockBuffer to compare two Attributes structs  
	*/
	int insertDone = 0;
	for(int index = 0; index < blockHeader.numEntries; index++) {
		Index leafEntry;
		leafBlk.getEntry(&leafEntry, index);
		if(compareAttrs(indexEntry.attrVal, leafEntry.attrVal, attrCatEntry.attrType) >= 0) {
			indices[index] = leafEntry;
		}
		else {
			//insert entry here and fill rest
			indices[index] = indexEntry;
			insertDone = 1;
			int blockIndex = index;
			index++;
			while(index <= blockHeader.numEntries) {
				leafBlk.getEntry(&leafEntry, blockIndex);
				indices[index] = leafEntry;
				index++;
				blockIndex++;
			}
			break;
		}
	}
	
	if(!insertDone) {
		indices[blockHeader.numEntries] = indexEntry;
	}
	
	if(blockHeader.numEntries != MAX_KEYS_LEAF) {
		//leaf block has not reached max limit 
		
		blockHeader.numEntries++;
		leafBlk.setHeader(&blockHeader);
		
		//iterate through all the entries of indices and setEntry
		for(int index=0; index < blockHeader.numEntries; index++) {
			leafBlk.setEntry(&indices[index], index);
		}
		
		return SUCCESS;
	}
	
	//if we reached here, the indices array has more than entries that can fit in a 
	//single leaf block. So split leaf
	int newRightBlk = BPlusTree::splitLeaf(blockNum, indices);
	
	if(newRightBlk == E_DISKFULL) return E_DISKFULL;
	
	if(blockHeader.pblock != -1) { //if current block was not root
		//insert middle value from indices into parent block using Internal Insert
		
		InternalEntry internalEntry;
		internalEntry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal;
		internalEntry.lChild = blockNum;
		internalEntry.rChild = newRightBlk;
		
		return insertIntoInternal(relId, attrName, blockHeader.pblock, internalEntry);
	}
	else {
	
		//current block was root and is now splitf
		//a new internal block must be created and made root
		
		return createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, blockNum, newRightBlk);
	}
	
	return SUCCESS;
}

int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {

	IndLeaf rightBlk;
	
	IndLeaf leftBlk(leafBlockNum);
	
	int rightBlkNum = rightBlk.getBlockNum();
	int leftBlkNum = leftBlk.getBlockNum();
	
	if(rightBlkNum == E_DISKFULL) return E_DISKFULL;
	
	//get headers
	HeadInfo leftBlkHeader, rightBlkHeader;
	rightBlk.getHeader(&rightBlkHeader);
	leftBlk.getHeader(&leftBlkHeader);
	
	//set right block header
	rightBlkHeader.numEntries = (MAX_KEYS_LEAF+1)/2;
	rightBlkHeader.pblock = leftBlkHeader.pblock;
	rightBlkHeader.lblock = leftBlkNum;
	rightBlkHeader.rblock = leftBlkHeader.rblock;
	rightBlk.setHeader(&rightBlkHeader);
	
	//set left block header
	leftBlkHeader.numEntries = (MAX_KEYS_LEAF+1)/2;
	leftBlkHeader.rblock = rightBlkNum;
	leftBlk.setHeader(&leftBlkHeader);
	
	for(int index = 0;index <= MIDDLE_INDEX_LEAF; index++) {
		leftBlk.setEntry(&indices[index], index);
		rightBlk.setEntry(&indices[index + MIDDLE_INDEX_LEAF + 1], index);
	}
	
	return rightBlkNum;
	
}

int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry) {

	AttrCatEntry attrCatEntry;
	AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
	
	IndInternal intBlk(intBlockNum);
	
	//get header
	HeadInfo blockHeader;
	intBlk.getHeader(&blockHeader);
	
	//declare internalEntries to store all existing entries + new entry
	InternalEntry internalEntries[blockHeader.numEntries + 1];
	
	//iterate through all entries and copy them to array
	int insertDone = 0,newIdx;
	for(int	index = 0; index < blockHeader.numEntries; index++) {
	
		InternalEntry internalEntry;
		intBlk.getEntry(&internalEntry, index);
		if( compareAttrs(intEntry.attrVal, internalEntry.attrVal, attrCatEntry.attrType) >= 0) {
			internalEntries[index] = internalEntry;
		}
		else {
			internalEntries[index] = intEntry;
			insertDone = 1;
			newIdx = index;
			int blockIndex = index;
			index++;
			while(index <= blockHeader.numEntries) {
				intBlk.getEntry(&internalEntry, blockIndex);
				internalEntries[index] = internalEntry;
				index++;
				blockIndex++;
			}
			break;
		}
	}
	
	if(!insertDone) {
		internalEntries[blockHeader.numEntries] = intEntry;
		newIdx = blockHeader.numEntries;
	}
	//update lchild of index after newIdx
	if(newIdx < blockHeader.numEntries) {
		internalEntries[newIdx+1].lChild = intEntry.rChild;
	}
	//update rchild of index before newIdx
	if(newIdx > 0)
		internalEntries[newIdx-1].rChild = intEntry.lChild;
	
	if(blockHeader.numEntries != MAX_KEYS_INTERNAL) {
	
		blockHeader.numEntries++;
		intBlk.setHeader(&blockHeader);
		
		for(int index=0; index<blockHeader.numEntries; index++) {
			intBlk.setEntry(&internalEntries[index], index);
		}
		
		return SUCCESS;
	}
	
	//if we reached here, internalEntries has more than MAX_KEYS_INTERNAL
	int newRightBlk = splitInternal(intBlockNum, internalEntries);
	
	if(newRightBlk == E_DISKFULL) {
		//BPlusDestroy
		BPlusTree::bPlusDestroy(intEntry.rChild);
		return E_DISKFULL;
	}
	
	//if current block was not root
	if(blockHeader.pblock != -1) {
	
		InternalEntry midEntry;
		midEntry.lChild = intBlockNum;
		midEntry.rChild = newRightBlk;
		midEntry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;
		
		return insertIntoInternal(relId, attrName, blockHeader.pblock, midEntry);
	}
	else {
		return createNewRoot(relId, attrName, internalEntries[MIDDLE_INDEX_INTERNAL].attrVal, intBlockNum, newRightBlk);
	}
	
	return SUCCESS;
}

int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]) {
	
	IndInternal rightBlk;
	IndInternal leftBlk(intBlockNum);
	
	int rightBlkNum = rightBlk.getBlockNum();
	int leftBlkNum = intBlockNum;
	
	if(rightBlkNum == E_DISKFULL) return E_DISKFULL;
	
	HeadInfo leftBlkHeader, rightBlkHeader;
	
	leftBlk.getHeader(&leftBlkHeader);
	rightBlk.getHeader(&rightBlkHeader);
	
	rightBlkHeader.numEntries = (MAX_KEYS_INTERNAL)/2;
	rightBlkHeader.pblock = leftBlkHeader.pblock;
	rightBlk.setHeader(&rightBlkHeader);
	
	leftBlkHeader.numEntries = (MAX_KEYS_INTERNAL)/2;
	leftBlkHeader.rblock = rightBlkNum;
	leftBlk.setHeader(&leftBlkHeader);
	
	//0 to 49 in lblock 51 to 100 in rblock
	for(int index=0; index < MIDDLE_INDEX_INTERNAL; index++) {
		leftBlk.setEntry(&internalEntries[index], index);
		rightBlk.setEntry(&internalEntries[index+MIDDLE_INDEX_INTERNAL+1], index);
	}
	
	int type = StaticBuffer::getStaticBlockType(internalEntries[0].lChild);
	
	//for all children of right block set parent as right block
	InternalEntry firstEntry;
	rightBlk.getEntry(&firstEntry, 0);
	
	BlockBuffer child(firstEntry.lChild);
	HeadInfo childHeader;
	child.getHeader(&childHeader);
	childHeader.pblock = rightBlkNum;
	child.setHeader(&childHeader);
	
	for(int index=0; index < MIDDLE_INDEX_INTERNAL; index++) {
	
		InternalEntry entry;
		rightBlk.getEntry(&entry, index);
		
		BlockBuffer child(entry.rChild);
		child.getHeader(&childHeader);
		childHeader.pblock = rightBlkNum;
		child.setHeader(&childHeader);
	}
	
	return rightBlkNum;
}

int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild) {

	AttrCatEntry attrCatEntry;
	AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
	
	IndInternal newRootBlk;
	
	int newRootBlkNum = newRootBlk.getBlockNum();
	
	if(newRootBlkNum == E_DISKFULL) {
	
		BPlusTree::bPlusDestroy(rChild);
		return E_DISKFULL;
	}
	
	HeadInfo newRootHeader;
	newRootBlk.getHeader(&newRootHeader);
	newRootHeader.numEntries = 1;
	newRootBlk.setHeader(&newRootHeader);
	
	InternalEntry newEntry;
	newEntry.lChild = lChild;
	newEntry.attrVal = attrVal;
	newEntry.rChild = rChild;
	
	newRootBlk.setEntry(&newEntry, 0);
	
	BlockBuffer leftChild(lChild);
	BlockBuffer rightChild(rChild);
	
	HeadInfo leftHeader, rightHeader;
	leftChild.getHeader(&leftHeader);
	rightChild.getHeader(&rightHeader);
	
	leftHeader.pblock = newRootBlkNum;
	rightHeader.pblock = newRootBlkNum;
	
	leftChild.setHeader(&leftHeader);
	rightChild.setHeader(&rightHeader);
	
	attrCatEntry.rootBlock = newRootBlkNum;
	AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
	
	return SUCCESS;
	
}
