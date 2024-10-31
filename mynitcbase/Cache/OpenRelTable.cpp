#include "OpenRelTable.h"
//#include "../Buffer/BlockBuffer.h"

#include<cstring>
#include<bits/stdc++.h>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

OpenRelTable::OpenRelTable() {
	
	//initialize relcache and attrcache with nullptr
	for(int i=0; i<MAX_OPEN; i++) {
		RelCacheTable::relCache[i] = nullptr;
		AttrCacheTable::attrCache[i] = nullptr;
		tableMetaInfo[i].free = true;
	}
	
	/************ Setting up Relation Cache entries ************/
	// (we need to populate relation cache with entries for the relation catalog
	//  and attribute catalog.)
	
	/**** setting up Relation Catalog relation in the Relation Cache Table****/
	RecBuffer relCatBlock(RELCAT_BLOCK);
	Attribute relCatRecord[RELCAT_NO_ATTRS];
	struct HeadInfo relCatHeader;
	
	relCatBlock.getHeader(&relCatHeader);
	
	for(int relid=0; relid<2; relid++) {
	
		relCatBlock.getRecord(relCatRecord, relid);
		
		struct RelCacheEntry relCacheEntry;
		RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
		relCacheEntry.recId.block = RELCAT_BLOCK;
		relCacheEntry.recId.slot = relid;
		
		RelCacheTable::relCache[relid] = (struct RelCacheEntry*)malloc(sizeof(struct RelCacheEntry));
		*(RelCacheTable::relCache[relid]) = relCacheEntry;
	}
	
	/************ Setting up Attribute cache entries ************/
	// (we need to populate attribute cache with entries for the relation catalog
	//  and attribute catalog.)
	
	//**** setting up Relation Catalog relation in the Attribute Cache Table ****/
	
	
	
	RecBuffer attrCatBlock(ATTRCAT_BLOCK);
	
	int slotNum = 0,currBlock = ATTRCAT_BLOCK;
	for(int relid = 0; relid<2;relid++) {
	
		Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
		struct AttrCacheEntry* head = (struct AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
		struct AttrCacheEntry* attrCacheEntry = head;
		
		int numAttrs = RelCacheTable::relCache[relid]->relCatEntry.numAttrs;
		
		for(int i=0; i<numAttrs; i++) {
	
			RecBuffer attrCatBlock(currBlock);		
			attrCatBlock.getRecord(attrCatRecord,slotNum);
			
			AttrCacheTable::recordToAttrCatEntry(attrCatRecord,&(attrCacheEntry->attrCatEntry));
			attrCacheEntry->recId.slot = slotNum;
			attrCacheEntry->recId.block = currBlock;
			slotNum++;
			
			if(i!=numAttrs-1) {
				attrCacheEntry->next = (struct AttrCacheEntry*)malloc(sizeof(struct AttrCacheEntry));
				attrCacheEntry=attrCacheEntry->next;
			}			
			
		}
		
		attrCacheEntry->next = nullptr;
		AttrCacheTable::attrCache[relid] = head;
		
		
	}
	
	/******* Setting up tableMetaInfo entries *******/
	
	tableMetaInfo[RELCAT_RELID].free = false;
	strcpy(tableMetaInfo[RELCAT_RELID].relName,RELCAT_RELNAME);
	
	tableMetaInfo[ATTRCAT_RELID].free = false;
	strcpy(tableMetaInfo[ATTRCAT_RELID].relName,ATTRCAT_RELNAME);
	

}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {
	
	for(int relId = 0; relId < MAX_OPEN; relId++) {
		if(!tableMetaInfo[relId].free && strcmp(tableMetaInfo[relId].relName,relName) == 0) {
			return relId;
		}
		
	}
		
	return E_RELNOTOPEN;	
	
}

int OpenRelTable::getFreeOpenRelTableEntry() {
	
	for(int relId=2; relId<MAX_OPEN; relId++) {
		
		if(tableMetaInfo[relId].free) {
			return relId;
		}	
		
	}		
	
	//no empty space in cache
	return E_CACHEFULL;
		
}

int OpenRelTable::openRel(char relName[ATTR_SIZE]) {
	
	//if relation already has an entry open in Open Relation Table return its rel-id
	int relId = getRelId(relName);
	if(relId != E_RELNOTOPEN) {
		return relId;
	}
	
	//find a free slot in OpenRelTable
	relId = getFreeOpenRelTableEntry();
	
	//if cache is full
	if(relId == E_CACHEFULL) {
		return E_CACHEFULL;
	}

	/******* Setting up Relation Cache Entry for the Relation ********/
	
	RelCacheTable::resetSearchIndex(RELCAT_RELID);
	
	Attribute attrVal;
	strcpy(attrVal.sVal,relName);
	
	RecId relcatRecId;	
		
	relcatRecId = BlockAccess::linearSearch(RELCAT_RELID,(char*)RELCAT_ATTR_RELNAME, attrVal, EQ);
	
	if(relcatRecId.block == -1 && relcatRecId.slot == -1) {
		return E_RELNOTEXIST;
	}
	
	//read the record entry corresponding to relcatRecId
	//create relCacheEntry on it using getRecord() and recordToRelCatEntry()
	RecBuffer relationBuffer(relcatRecId.block);
	
	Attribute relCatRecord[RELCAT_NO_ATTRS];
	
	relationBuffer.getRecord(relCatRecord,relcatRecId.slot);
	
	RelCacheEntry *relCacheEntry;
	relCacheEntry = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
	
	RelCacheTable::recordToRelCatEntry(relCatRecord, &(relCacheEntry->relCatEntry));
	
	//update rec-id of RelCacheEntry
	relCacheEntry->recId.block = relcatRecId.block;
	relCacheEntry->recId.slot = relcatRecId.slot;
	
	RelCacheTable::relCache[relId] = relCacheEntry;
  	/****** Setting up Attribute Cache entry for the relation ******/
	
	
	/*iterate over all the entries in the Attribute Catalog corresponding to each
  	attribute of the relation relName by multiple calls of BlockAccess::linearSearch()
  	care should be taken to reset the searchIndex of the relation, ATTRCAT_RELID,
  	corresponding to Attribute Catalog before the first call to linearSearch().*/

	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
	
	AttrCacheEntry *listHead = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
	AttrCacheEntry *attrCacheEntry = listHead;
	
	int numAttrs = RelCacheTable::relCache[relId]->relCatEntry.numAttrs;
	
	for(int i=0; i<numAttrs; i++) {
		
		//attrcatRecId stores a valid recid of an entry of relation relName in Attribute Catalog
		RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, attrVal ,EQ);
		
		RecBuffer attrCatBlock(attrcatRecId.block);
		Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
		
		attrCatBlock.getRecord(attrCatRecord, attrcatRecId.slot);
		
		AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));
		attrCacheEntry->recId.block = attrcatRecId.block;
		attrCacheEntry->recId.slot = attrcatRecId.slot;
		
		if(i != numAttrs-1) {
			attrCacheEntry->next = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
			attrCacheEntry = attrCacheEntry->next;
		}
		
	}
	
	attrCacheEntry->next = nullptr;
	AttrCacheTable::attrCache[relId] = listHead;
	
	
	/****** Setting up metadata in the Open Relation Table for the relation******/
	
	tableMetaInfo[relId].free = false;
	
	strcpy(tableMetaInfo[relId].relName, relName);
	
	return relId;
	
}

int OpenRelTable::closeRel(int relId) {
	
	if(relId == RELCAT_RELID || relId == ATTRCAT_RELID) {
		return E_NOTPERMITTED;
	}
	
	if(relId < 0 || relId >= MAX_OPEN) {
		return E_OUTOFBOUND;
	}
	
	if(tableMetaInfo[relId].free) {
		return E_RELNOTOPEN;
	}
  	
  	/************  Releasing the Relation Cache entry of the relation   **************/
  	
  	if(RelCacheTable::relCache[relId]->dirty) {
  	
  		//get the relation catalog entry from rel Cache Table then convert it into a record
  		Attribute relCatRecord[RELCAT_NO_ATTRS];
  		RelCacheTable::relCatEntryToRecord(&(RelCacheTable::relCache[relId]->relCatEntry),relCatRecord);
  		
  		
  		//declaring an object of RecBuffer class to write back to the buffer
  		RecId recId = RelCacheTable::relCache[relId]->recId;
  		RecBuffer relCatBlock(recId.block);
  		
  		relCatBlock.setRecord(relCatRecord,recId.slot);
  	
  	}
  	
  	free(RelCacheTable::relCache[relId]);
  	
  	/************ Releasing the Attribute Cache Entry of the relation ***************/
  	
  	//for all entries in the linked list of the relidth Attrbiute Cache Entry
  	AttrCacheEntry *entry = AttrCacheTable::attrCache[relId];
  	while(entry != nullptr) {
  	
  		if(entry->dirty) {
  			//get attrCatEntry from attrCache
  			AttrCatEntry attrCatEntry = entry->attrCatEntry;
  			//convert it to record and write back
  			Attribute record[ATTRCAT_NO_ATTRS];
  			AttrCacheTable::attrCatEntryToRecord(&attrCatEntry, record);
  			
  			RecBuffer attrCat(entry->recId.block);
  			attrCat.setRecord(record, entry->recId.slot);
  		}
  		
  		AttrCacheEntry *next = entry->next;
  		free(entry);
  		entry = nullptr;
  		entry = next;
  	}
  	
  	
  	//update `tableMetaInfo` to set `relId` as a free slot
  	//update `relCache` and `attrCache` to set the entry at relId to nullptr
  	
  	tableMetaInfo[relId].free = true;
  	
  	RelCacheTable::relCache[relId] = nullptr;
  	AttrCacheTable::attrCache[relId] = nullptr;
  	
  	return SUCCESS;
}

OpenRelTable::~OpenRelTable() {

	//close all open relations from rel-id =2 onwards 
	for(int i=2; i<MAX_OPEN; i++) {
		if(!tableMetaInfo[i].free) {
			OpenRelTable::closeRel(i); 
		}
	}	
	
	
	/******** Closing the catalog entries in the relation cache *********/
	
	//releasing the relation cache entry of attribute catalog
	if(RelCacheTable::relCache[ATTRCAT_RELID]->dirty) {
		
		//get relCatEntry of attribute cat
		RelCatEntry relCatEntryOfAttrCat;
		RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntryOfAttrCat);
		
		//convert to record
		Attribute relCatRecord[RELCAT_NO_ATTRS];
		RelCacheTable::relCatEntryToRecord(&relCatEntryOfAttrCat, relCatRecord);
		
		RecId recId = RelCacheTable::relCache[ATTRCAT_RELID]->recId;
		//declaring an object of RecBuffer class to write back to the buffer
		RecBuffer relCatBlock(recId.block);
		
		//write back to the buffer using relCatBlock.setRecord() with recId.slot
		relCatBlock.setRecord(relCatRecord, recId.slot);
	
	}
	
	// free all the memory that you allocated in the constructor
	free(RelCacheTable::relCache[ATTRCAT_RELID]);
	
	//releasing the relation cache entry of the relation catalog
	if(RelCacheTable::relCache[RELCAT_RELID]->dirty) {
	
		//get relCatEntry of relation cat
		RelCatEntry relCatEntryOfRelCat;
		RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntryOfRelCat);
		
		//convert it to record
		Attribute relCatRecord[RELCAT_NO_ATTRS];
		RelCacheTable::relCatEntryToRecord(&relCatEntryOfRelCat, relCatRecord);
		
		//write back this record to block
		RecId recId = RelCacheTable::relCache[RELCAT_RELID]->recId;
		RecBuffer relCatBlock(recId.block);
		
		relCatBlock.setRecord(relCatRecord, recId.slot);
	
	}
	
	free(RelCacheTable::relCache[RELCAT_RELID]);
	
  	AttrCacheEntry *head = AttrCacheTable::attrCache[RELCAT_RELID];
  	AttrCacheEntry *next = head->next;
  	
  	while(head->next) {
  		free(head);
  		head = next;
  		next = next->next;
  	}
  	free(head);
  	
   	head = AttrCacheTable::attrCache[ATTRCAT_RELID];
  	next = head->next;
  	
  	while(head->next) {
  		free(head);
  		head = next;
  		next = next->next;
  	}
  	free(head);
	
}



