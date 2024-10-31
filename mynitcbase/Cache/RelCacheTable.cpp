#include "RelCacheTable.h"

#include <cstring>
#include<bits/stdc++.h>

RelCacheEntry* RelCacheTable::relCache[MAX_OPEN];


/*
Get the relation catalog entry for the relation with rel-id `relId` from the cache
NOTE: this function expects the caller to allocate memory for `*relCatBuf`
*/
int RelCacheTable::getRelCatEntry(int relId, RelCatEntry* relCatBuf) {
	if(relId < 0 ||relId >= MAX_OPEN) {
		return E_OUTOFBOUND;
	}
	
	//if there is no entry at the relId
	if(relCache[relId] ==nullptr) {
		return E_RELNOTOPEN;
	}
	
	//copy the value to the relCatBuf argument
	*relCatBuf = relCache[relId]->relCatEntry;
	
	return SUCCESS;
}

int RelCacheTable::setRelCatEntry(int relId,RelCatEntry* relCatBuf) {

	if(relId<0 || relId>= MAX_OPEN) {
		return E_OUTOFBOUND;
	}
	
	if(relCache[relId] == nullptr) {
		return E_RELNOTOPEN;
	}
	
	//copy the relCatBuf to the corresponding Relation Catelog Entry in relCache Table
	relCache[relId]->relCatEntry = *relCatBuf;
	
	
	//set dirty flag of corresponding rel Cache Entry in Rel Cache Table
	relCache[relId]->dirty = true;
	
	return SUCCESS;
}

/* Converts a relation catalog record to RelCatEntry struct
    We get the record as Attribute[] from the BlockBuffer.getRecord() function.
    This function will convert that to a struct RelCatEntry type.
NOTE: this function expects the caller to allocate memory for `*relCatEntry`
*/
void RelCacheTable::recordToRelCatEntry(union Attribute record[RELCAT_NO_ATTRS], RelCatEntry *relCatEntry) {
 	
 	strcpy(relCatEntry->relName,record[RELCAT_REL_NAME_INDEX].sVal);
 	relCatEntry->numAttrs = (int)record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
 	relCatEntry->numRecs = (int)record[RELCAT_NO_RECORDS_INDEX].nVal;
 	relCatEntry->firstBlk = (int)record[RELCAT_FIRST_BLOCK_INDEX].nVal;
 	relCatEntry->lastBlk = (int)record[RELCAT_LAST_BLOCK_INDEX].nVal;
 	relCatEntry->numSlotsPerBlk = (int)record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal;
 	
 }


void RelCacheTable::relCatEntryToRecord(RelCatEntry* relCatEntry, union Attribute record[RELCAT_NO_ATTRS]) {

	strcpy(record[RELCAT_REL_NAME_INDEX].sVal, relCatEntry->relName);
	record[RELCAT_NO_ATTRIBUTES_INDEX].nVal = (int)relCatEntry->numAttrs;
	record[RELCAT_NO_RECORDS_INDEX].nVal = (int)relCatEntry->numRecs;
	record[RELCAT_FIRST_BLOCK_INDEX].nVal = (int)relCatEntry->firstBlk;
	record[RELCAT_LAST_BLOCK_INDEX].nVal = (int)relCatEntry->lastBlk;
	record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal= (int)relCatEntry->numSlotsPerBlk;

}
/* will return the searchIndex for the relation corresponding to `relId
NOTE: this function expects the caller to allocate memory for `*searchIndex`
*/
int RelCacheTable::getSearchIndex(int relId, RecId* searchIndex) {
	if(relId<0 || relId >= MAX_OPEN) {
		return E_OUTOFBOUND;
	}
	
	if(relCache[relId] == nullptr) {
		return E_RELNOTOPEN;
	}
	
	*searchIndex = relCache[relId]->searchIndex;
	return SUCCESS;
}


// sets the searchIndex for the relation corresponding to relId
int RelCacheTable::setSearchIndex(int relId, RecId* searchIndex) {
	if(relId < 0 || relId >= MAX_OPEN) {
		return E_OUTOFBOUND;
	}
	
	if(relCache[relId] == nullptr) {
		return E_RELNOTOPEN;
	}
	
	relCache[relId]->searchIndex = *searchIndex;
	
	return SUCCESS;
}

int RelCacheTable::resetSearchIndex(int relId) {

	if(relId < 0 || relId >= MAX_OPEN) {
		return E_OUTOFBOUND;
	}
	
	if(relCache[relId] == nullptr) {
		return E_RELNOTOPEN;
	}
	
	relCache[relId]->searchIndex.block = -1;
	relCache[relId]->searchIndex.slot = -1;
	
	return SUCCESS;
	
}
