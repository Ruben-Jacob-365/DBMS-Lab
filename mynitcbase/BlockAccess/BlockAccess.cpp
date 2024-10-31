#include "BlockAccess.h"
#include<bits/stdc++.h>
#include <cstring>
#include<stdio.h>

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) {

	//get previous search index of the relation relid
	RecId prevRecId;
	RelCacheTable::getSearchIndex(relId,&prevRecId);
	int block,slot;
	
	// if the current search index record is invalid(i.e. both block and slot = -1)
	if(prevRecId.block == -1 && prevRecId.slot == -1) {
		// no hits from previous search; search should start from first record itself
		RelCatEntry relCatEntry;
		RelCacheTable::getRelCatEntry(relId, &relCatEntry);
		
		block = relCatEntry.firstBlk;
		slot=0;
	}
	else {
		//there is a hit from previos search; search should start from the
		//record next to the search index record
		
		block = prevRecId.block;
		slot = prevRecId.slot+1;
	}
	
	/* The following code searches for the next record in the relation
       that satisfies the given condition
       We start from the record id (block, slot) and iterate over the remaining
       records of the relation
    */   
	
	while(block!=-1) {
		///* create a RecBuffer object for block
		RecBuffer blockBuff(block);
		
		//get header of block
		HeadInfo blockHeader;
		blockBuff.getHeader(&blockHeader);

		
		//get slot map of the block
		unsigned char slotMap[blockHeader.numSlots];
		blockBuff.getSlotMap(slotMap);
		
		//if no more slots in this block go to next block
		if(slot >= blockHeader.numSlots) {
			block = blockHeader.rblock;
			slot = 0;
			continue;
		}
		
		//if slot is free skip the loop
		if(slotMap[slot] == SLOT_UNOCCUPIED) {
			slot+=1;
			continue;
		}
		
		//get the record with id (block,slot)
		Attribute record[blockHeader.numAttrs];		
		blockBuff.getRecord(record,slot);
		
		//compare record's attribute value to the given attrVal
		AttrCatEntry attrCatBuffer;
		AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatBuffer);
		
		int attrOffset = attrCatBuffer.offset;		
		
		/* use the attribute offset to get the value of the attribute from
           current record */
		Attribute currAttrVal = record[attrOffset];
		
		//set cmpval to store difference between attributes
		int cmpVal;
		cmpVal = compareAttrs(currAttrVal, attrVal, attrCatBuffer.attrType);
		
		//check whether this record satisfies the given condition
		if (
			(op == NE && cmpVal != 0) ||
			(op == LT && cmpVal < 0) ||
			(op == LE && cmpVal <= 0) ||
			(op == EQ && cmpVal == 0) ||
			(op == GT && cmpVal > 0) ||
			(op == GE && cmpVal >= 0)
		) {
			RecId recId = {block,slot};
			RelCacheTable::setSearchIndex(relId,&recId);
			
			return recId;
		}
		
		slot++;
	}
	
	//no record in the relation with Id relid satisfies the given condition
	return RecId{-1,-1};
}

int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {
	
	//reset the search index of relation catalog using RelCacheTable::resetSearchIndex
	RelCacheTable::resetSearchIndex(RELCAT_RELID);
	
	Attribute newRelationName;
	strcpy(newRelationName.sVal,newName);
	
	//search the relation catalog for an entry with "RelName" = newRelationName
	RecId searchIndex = BlockAccess::linearSearch(RELCAT_RELID,(char*)RELCAT_ATTR_RELNAME,newRelationName,EQ);
	
	if(searchIndex.block!=-1 && searchIndex.slot!=-1) {
		return E_RELEXIST;
	}
	
	//reset searchIndex of Relation Catalog again
	Attribute oldRelationName;
	strcpy(oldRelationName.sVal,oldName);
	
	searchIndex = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, oldRelationName, EQ);
	
	if(searchIndex.block == -1 && searchIndex.slot == -1) {
		return E_RELNOTEXIST;
	}
	
	RecBuffer relationCat(RELCAT_BLOCK);
	Attribute relCatRecord[RELCAT_NO_ATTRS];
	
	//get relcatrecord of relation to rename
	relationCat.getRecord(relCatRecord, searchIndex.slot);
	
	strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, newName);
	
	//set back the record
	relationCat.setRecord(relCatRecord, searchIndex.slot);
	
	/**** update all attributes of this relation to have newrel name ****/
	
	//reset search index of attribute catalog
	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
	
	int numAttrs = relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
	
	for(int i=0; i<numAttrs; i++) {
		searchIndex = BlockAccess::linearSearch(ATTRCAT_RELID,(char*)ATTRCAT_ATTR_RELNAME, oldRelationName, EQ);
		
		RecBuffer attrCat(searchIndex.block);
		Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
		
		//get record
		attrCat.getRecord(attrCatRecord, searchIndex.slot);
		
		strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);
		
		//set the record
		attrCat.setRecord(attrCatRecord,searchIndex.slot);
		
	}
	
	return SUCCESS;
}

int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {

	//reset searchIndex of relation cat
	RelCacheTable::resetSearchIndex(RELCAT_RELID);
	
	Attribute relNameAttr;
	
	strcpy(relNameAttr.sVal,relName);
	
	RecId searchIndex = BlockAccess::linearSearch(RELCAT_RELID,(char*)RELCAT_ATTR_RELNAME,relNameAttr, EQ);
	
	if(searchIndex.block == -1 && searchIndex.slot == -1) {
		return E_RELNOTEXIST;
	}
	
	//reset search index of attribute catalog
	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
	
	//declare attrToRenameRecId used to store red-id of attribute to rename
	RecId attrToRenameRecId{-1,-1};
	Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];
	
	//iterate over all Attribute Catalog Entry record corresponding to the relation to find the require attribute
	
	while(true) {
		
		searchIndex = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);
		
		if(searchIndex.block == -1 && searchIndex.slot == -1) {
			break;
		}
		
		RecBuffer attrCatBlock(searchIndex.block);
		attrCatBlock.getRecord(attrCatEntryRecord,searchIndex.slot);
		
		if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,oldName) == 0) {
			attrToRenameRecId = searchIndex;
			break;
		}
		
		if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,newName)==0) {
			return E_ATTREXIST;
		}
		
	}
	
	if(attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1) {
		return E_ATTRNOTEXIST;
	}
	
	//update entry corresponding to the attribute in attribute catalog relation
	
	RecBuffer attrCatBlock(attrToRenameRecId.block);
	
	attrCatBlock.getRecord(attrCatEntryRecord,attrToRenameRecId.slot);
	
	strcpy(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,newName);
	
	attrCatBlock.setRecord(attrCatEntryRecord,attrToRenameRecId.slot);
	
	return SUCCESS;

}

int BlockAccess::insert(int relId, Attribute *record) {

	//get relation catelog entry from relation cache
	RelCatEntry relCatEntry;
	RelCacheTable::getRelCatEntry(relId, &relCatEntry);
	
	int blockNum = relCatEntry.firstBlk;
	
	//rec_id will be used to store where the new record will be inserted
	RecId rec_id = {-1,-1};
	
	int numOfSlots = relCatEntry.numSlotsPerBlk;
	int numOfAttributes = relCatEntry.numAttrs;
	
	//block number of the last element in the linked list=-1
	int prevBlockNum = -1;
	
	/*
            Traversing the linked list of existing record blocks of the relation
            until a free slot is found OR
            until the end of the list is reached
    	*/
    	
    	while(blockNum != -1) {
    	
    		RecBuffer currBlock(blockNum);
    		HeadInfo currBlockHeader;
    		currBlock.getHeader(&currBlockHeader);
    		
    		unsigned char slotMap[numOfSlots];
    		currBlock.getSlotMap(slotMap);
    		
    		//search for a free slot in the current block and store its recId in rec_id
    		for(int slot = 0; slot<numOfSlots ; slot++) {
    			
    			if(slotMap[slot] == SLOT_UNOCCUPIED) {
    			
    				rec_id = RecId{blockNum,slot};
    				break;
    			
    			}
    			
    		}
    		
    		if(rec_id.block!=-1 && rec_id.slot!=-1) {
    			break;
    		}
    		
    		//if no free slots in current block go to next block in linked list
    		prevBlockNum = blockNum;
    		blockNum = currBlockHeader.rblock;
    	
    	}
    	
    	if(rec_id.block == -1 && rec_id.slot == -1) {
    	
    		if(relId == RELCAT_RELID) {
    			return E_MAXRELATIONS;
    		}
    		
    		//get a new record block
    		RecBuffer newBlock;
    		
    		//get block Num of newly allocated block
    		int ret = newBlock.getBlockNum();
    		
    		if(ret == E_DISKFULL) {
    			return E_DISKFULL;    		
    		}
    		
    		int blockNum = ret;
    		
    		//Assign rec_id.block = new block number(ie ret) and rec_id.slot =0
    		rec_id.block = blockNum;
    		rec_id.slot = 0;
    		
    		//set header to new block
    		HeadInfo header;
    		
    		header.blockType = REC;
    		header.pblock = -1;
    		header.lblock = prevBlockNum;
    		header.rblock = -1;
    		header.numEntries = 0;
    		header.numSlots = numOfSlots;
    		header.numAttrs = numOfAttributes;
    		
    		newBlock.setHeader(&header);
    		
    		//set block's slot map
    		unsigned char slotMap[numOfSlots];
    		for(int slot=0; slot<numOfSlots; slot++) {
    			slotMap[slot] = SLOT_UNOCCUPIED;
    		}
    		
    		newBlock.setSlotMap(slotMap);
    		
    		if(prevBlockNum != -1) {
    		
    			//get prev Block
    			RecBuffer prevBlock(prevBlockNum);
    			
    			//update its header to have rblock = newBlock
    			HeadInfo prevBlockHeader;
    			
    			prevBlock.getHeader(&prevBlockHeader);
    			prevBlockHeader.rblock = blockNum;
    			
    			prevBlock.setHeader(&prevBlockHeader);
    		
    		}
    		
    		else {
    			//no prev block means this is the first block
    			relCatEntry.firstBlk = blockNum;
    			RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    		}
    		
    		relCatEntry.lastBlk = blockNum;
    		RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    		
    	}
    	
    	//create a RecBuffer object for rec_id.block
    	RecBuffer insertBlock(rec_id.block);
    	
    	insertBlock.setRecord(record, rec_id.slot);
    	
    	//update slotMap of block by marking entry of the slot as SLOT_OCCUPIED
    	
    	unsigned char slotMap[numOfSlots];
    	
    	insertBlock.getSlotMap(slotMap);
    	slotMap[rec_id.slot] = SLOT_OCCUPIED;
    	insertBlock.setSlotMap(slotMap);
    	
    	//increment the numEntries field of block's header
    	HeadInfo insertBlockHeader;
    	
    	insertBlock.getHeader(&insertBlockHeader);
    	insertBlockHeader.numEntries += 1;
    	insertBlock.setHeader(&insertBlockHeader);
    	
    	relCatEntry.numRecs++;
    	RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    	
    	/* B+ tree insertions */
    	
    	int flag = SUCCESS;
    	//iterate over all attributes of the relation
    	for(int attrOffset = 0; attrOffset < numOfAttributes; attrOffset++) {
    	
    		AttrCatEntry attrCatEntry;
    		AttrCacheTable::getAttrCatEntry(relId, attrOffset, &attrCatEntry);
    		
    		int rootBlock = attrCatEntry.rootBlock;
    		if(rootBlock != -1) {
    			int retVal = BPlusTree::bPlusInsert(relId, attrCatEntry.attrName, record[attrOffset], rec_id);
    			if(retVal == E_DISKFULL)
    				flag = E_INDEX_BLOCKS_RELEASED;
    		}
    	}
    	
    	return flag;

}

int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op) {

	//Declare a variable called recid to store the searched record
	RecId recId;
	
	/* get the attibute catalog entry from the attribute cache corresponding to the relation with id = relId with attrName */
	AttrCatEntry attrCatEntry;
	int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
	if(ret != SUCCESS) return ret;
	
	int rootBlock = attrCatEntry.rootBlock;
	
	if(rootBlock == -1) {

		/* search for the record id (recid) corresponding to the attribute with
	    	attribute name attrName, with value attrval and satisfying the condition op
	    	using linearSearch() */
	    	recId = BlockAccess::linearSearch(relId, attrName, attrVal, op);
	}
	else {
		/* search for the record id corresponding to the attribute using BPlusSearch */
		recId = BPlusTree::bPlusSearch(relId, attrName, attrVal, op);
	}
	    	
	// if no record satisfies
    	if(recId.block == -1 && recId.slot == -1)
    		return E_NOTFOUND;
    	
    	//copy the (block, slot) to record
    	RecBuffer block(recId.block);
    	block.getRecord(record,recId.slot);
    	
    	return SUCCESS;

}

int BlockAccess::deleteRelation(char relName[ATTR_SIZE]){ 

	if(strcmp(RELCAT_RELNAME,relName) == 0 || strcmp(ATTRCAT_RELNAME,relName) == 0)
		return E_NOTPERMITTED;
	//reset the searchIndex of relation catalog
	RelCacheTable::resetSearchIndex(RELCAT_RELID);
	
	Attribute relNameAttr;
	strcpy(relNameAttr.sVal, relName);

	
	RecId recId = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, relNameAttr, EQ);
	
	
	if(recId.block == -1 && recId.slot == -1) 
		return E_RELNOTEXIST;
	
	//store the relation catalog record corresponding to the relation in relCatEntryRecord
	RecBuffer relCatBlock(recId.block);
	Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
	
	relCatBlock.getRecord(relCatEntryRecord, recId.slot);
	
	int firstBlock = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
	int numAttrs = relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
	
	/*        Delete all record blocks of the relation         */
	
	int currBlockNum = firstBlock;
	while(currBlockNum != -1) {
	
		RecBuffer currBlock(currBlockNum);
		
		HeadInfo currBlockHeader;
		currBlock.getHeader(&currBlockHeader);
		
		currBlockNum = currBlockHeader.rblock;
		
		currBlock.releaseBlock();
	
	}
	
	/******    Deleting attribute catalog entries    *******/
	
	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
	
	int numberOfAttributesDeleted = 0;
	
	while(true) {
	
		RecId attrCatRecId;
		attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);
		
		if(attrCatRecId.block == -1 && attrCatRecId.slot == -1) {
			break;
		}
		
		numberOfAttributesDeleted++;
		
		//create a recBuffer for attrCatRecId.block
		RecBuffer attrCatBlock(attrCatRecId.block);
		
		//get header
		HeadInfo attrCatBlockHeader;
		attrCatBlock.getHeader(&attrCatBlockHeader);
		
		//get record
		Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
		attrCatBlock.getRecord(attrCatRecord, attrCatRecId.slot);
		
		//declare variable rootBlock to store rootBlock field
		int rootBlock = attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
		//(will be used lated to delete any indexes if it exists)
		
		//Update the slotMap for the block by setting the slot as SLOT_UNOCCUPIED
		unsigned char slotMap[attrCatBlockHeader.numSlots];
		
		attrCatBlock.getSlotMap(slotMap);
		slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
		attrCatBlock.setSlotMap(slotMap);
		
		/* Decrement the numEntries in the header of the block corresponding to
           	the attribute catalog entry and then set back the header
           	using RecBuffer.setHeader */
           	
           	attrCatBlockHeader.numEntries -= 1;
           	attrCatBlock.setHeader(&attrCatBlockHeader);
           	//if numEntries become 0 releaseBlock is called after fixing the linked list
           	
           	if(attrCatBlockHeader.numEntries == 0) {
           		/* Standard Linked List Delete for a Block
               		Get the header of the left block and set it's rblock to this
               		block's rblock
            		*/
            		RecBuffer prevBlock(attrCatBlockHeader.lblock);
            		
            		HeadInfo prevBlockHeader;
            		prevBlock.getHeader(&prevBlockHeader);
            		prevBlockHeader.rblock = attrCatBlockHeader.rblock;
            		prevBlock.setHeader(&prevBlockHeader);
            		
            		if(attrCatBlockHeader.rblock != -1) {
            		
            			//get header of right block and set its lblock to current blocks lblock
            			RecBuffer nextBlock(attrCatBlockHeader.rblock);
            			HeadInfo nextBlockHeader;
            			
            			nextBlock.getHeader(&nextBlockHeader);
            			nextBlockHeader.lblock = attrCatBlockHeader.lblock;
            			nextBlock.setHeader(&nextBlockHeader);
            			
            		}
            		else {
            			//the block being released is the last block of the attrCatelo  
            			//update the Relation Catalog entry's lastBlk field
            			RelCatEntry relCatEntry;
            			RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntry);
            			
            			relCatEntry.lastBlk = attrCatBlockHeader.lblock;
            			RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relCatEntry);
            		} 
            		
            		attrCatBlock.releaseBlock();
           	}
           	
           	if(rootBlock != -1) {
           		BPlusTree::bPlusDestroy(rootBlock);
           	}
           	
           	
	}
	
	/*** Delete the entry corresponding to the relation from relation catalog ***/
	
	//fetch header of relCatBlock
	HeadInfo relCatHeader;
	
	relCatBlock.getHeader(&relCatHeader);
	relCatHeader.numEntries--;
	relCatBlock.setHeader(&relCatHeader);
	
	//get slotmap of relCatBlock and update it
	unsigned char slotMap[relCatHeader.numSlots];
	relCatBlock.getSlotMap(slotMap);
	slotMap[recId.slot] = SLOT_UNOCCUPIED;
	relCatBlock.setSlotMap(slotMap);
	
	/****   Updating the relation Cache Table   ****/
	RelCatEntry relCatEntry;
	
	RelCacheTable::getRelCatEntry(RELCAT_RELID,&relCatEntry);
	
	relCatEntry.numRecs--;	
	RelCacheTable::setRelCatEntry(RELCAT_RELID,&relCatEntry);
	
	/*** Updating attribute catalog entry ***/
	RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntry);
	relCatEntry.numRecs -= numberOfAttributesDeleted;
	RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relCatEntry);
	
	return SUCCESS;

}

int BlockAccess::project(int relId, Attribute* record) {

	//get the previous search Index of the relation relid from the realtion cache
	RecId prevRecId;
	RelCacheTable::getSearchIndex(relId, &prevRecId);
	
	//declare block and slot which will be used to store RecId of slot we need to check
	int block,slot;
	
	//if current search index is invalid -- only when caller reset search index
	if(prevRecId.block == -1 && prevRecId.slot == -1) {
		//new project operation - start from beginning
		RelCatEntry relCatEntry;
		RelCacheTable::getRelCatEntry(relId, &relCatEntry);
		
		block = relCatEntry.firstBlk;
		slot = 0;
	}
	else {
		//a project/search operation is alreadt in progress
		block = prevRecId.block;
		slot = prevRecId.slot + 1;
	}
	
	//the following code finds the next record of the relation
	while(block != -1){
	
		//create a RecBuffer object for block
		RecBuffer currBlock(block);
		
		//get header and slotmap
		HeadInfo currBlockHeader;
		currBlock.getHeader(&currBlockHeader);
		
		unsigned char slotMap[currBlockHeader.numSlots];
		currBlock.getSlotMap(slotMap);
		
		if(slot >= currBlockHeader.numSlots) {
			// no more slots in block
			block = currBlockHeader.rblock;
			slot = 0;
		}
		else if(slotMap[slot] == SLOT_UNOCCUPIED){
			//increment slot
			slot++;
		}
		else{
			//next occupied slot/record has been found
			break;
		}
	}
	
	if(block == -1){
		//a record was not found - all records exhausted
		return E_NOTFOUND;
	}
	
	//declare nextRecId to store the RecId of the record found
	RecId nextRecId{block,slot};
	
	//set the search Index to nextRecId 
	RelCacheTable::setSearchIndex(relId, &nextRecId);
	
	RecBuffer reqBlock(block);
	reqBlock.getRecord(record, slot);
	
	return SUCCESS;
}

