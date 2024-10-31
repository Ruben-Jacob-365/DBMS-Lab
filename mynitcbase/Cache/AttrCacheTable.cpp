#include "AttrCacheTable.h"

#include <cstring>
#include<bits/stdc++.h>

AttrCacheEntry* AttrCacheTable::attrCache[MAX_OPEN];

/* returns the attrOffset-th attribute for the relation corresponding to relId
NOTE: this function expects the caller to allocate memory for `*attrCatBuf`
*/

int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry* attrCatBuf) {
	
	if(relId<0 || relId>=MAX_OPEN) {
		return E_OUTOFBOUND;
	}
	
	if(attrCache[relId] == nullptr) {
		return E_RELNOTOPEN;
	}
	
	//traverse the linked list of attribute cache entries
	for(AttrCacheEntry* entry = attrCache[relId]; entry!=nullptr; entry=entry->next) {
		if(entry->attrCatEntry.offset == attrOffset) {
		
			//*attrCatBuf = entry->attrCatEntry
			strcpy(attrCatBuf->relName, entry->attrCatEntry.relName);
        		strcpy(attrCatBuf->attrName, entry->attrCatEntry.attrName);
            		attrCatBuf->attrType = entry->attrCatEntry.attrType;
            		attrCatBuf->primaryFlag = entry->attrCatEntry.primaryFlag;
            		attrCatBuf->rootBlock = entry->attrCatEntry.rootBlock;
            		attrCatBuf->offset = entry->attrCatEntry.offset;
            		
			return SUCCESS;
		}
	}
	
	//there is no attribute at this offset
	return E_ATTRNOTEXIST;
}

/* returns the attribute with name `attrName` for the relation corresponding to relId
NOTE: this function expects the caller to allocate memory for `*attrCatBuf`
*/
int AttrCacheTable::getAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry* attrCatBuf) {

	if(relId<0 || relId >= MAX_OPEN) {
		return E_OUTOFBOUND;
	}
	
	if(attrCache[relId] == nullptr) {
		return E_RELNOTOPEN;
	}
	
	for(AttrCacheEntry* entry = attrCache[relId]; entry!=nullptr; entry = entry->next) {
		if(strcmp(entry->attrCatEntry.attrName,attrName) == 0) {
			//*attrCatBuf = entry->attrCatEntry
			strcpy(attrCatBuf->relName, entry->attrCatEntry.relName);
        		strcpy(attrCatBuf->attrName, entry->attrCatEntry.attrName);
            		attrCatBuf->attrType = entry->attrCatEntry.attrType;
            		attrCatBuf->primaryFlag = entry->attrCatEntry.primaryFlag;
            		attrCatBuf->rootBlock = entry->attrCatEntry.rootBlock;
            		attrCatBuf->offset = entry->attrCatEntry.offset;
            		
			return SUCCESS;
			
		}
	}
	
	//no attribute with name attrName for the relation
	return E_ATTRNOTEXIST;
}


int AttrCacheTable::setAttrCatEntry(int relId, int attrOffset, AttrCatEntry *attrCatBuff) {

	if(relId < 0 || relId >= MAX_OPEN) return E_OUTOFBOUND;
	
	if(attrCache[relId] == nullptr) return E_RELNOTOPEN;
	
	for(AttrCacheEntry *entry = attrCache[relId]; entry!=nullptr; entry = entry->next){
	
		if(entry->attrCatEntry.offset == attrOffset) {
			
			//entry->attrCatEntry = *attrCatBuff;
			strcpy(entry->attrCatEntry.relName, attrCatBuff->relName);
			strcpy(entry->attrCatEntry.attrName, attrCatBuff->attrName);
			entry->attrCatEntry.attrType = attrCatBuff->attrType;
			entry->attrCatEntry.rootBlock = attrCatBuff->rootBlock;
			entry->attrCatEntry.primaryFlag = attrCatBuff->primaryFlag;
			entry->attrCatEntry.offset = attrCatBuff->offset;
			
			entry->dirty = true;
			
			return SUCCESS;
		}
	}
	
	return E_ATTRNOTEXIST;
}

int AttrCacheTable::setAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry *attrCatBuff) {

	if(relId < 0 || relId >= MAX_OPEN) return E_OUTOFBOUND;
	
	if(attrCache[relId] == nullptr) return E_RELNOTOPEN;
	
	for(AttrCacheEntry *entry = attrCache[relId]; entry!=nullptr; entry = entry->next){
	
		if(strcmp(entry->attrCatEntry.attrName, attrName) == 0) {
			
			//entry->attrCatEntry = *attrCatBuff;
			strcpy(entry->attrCatEntry.relName, attrCatBuff->relName);
			strcpy(entry->attrCatEntry.attrName, attrCatBuff->attrName);
			entry->attrCatEntry.attrType = attrCatBuff->attrType;
			entry->attrCatEntry.rootBlock = attrCatBuff->rootBlock;
			entry->attrCatEntry.primaryFlag = attrCatBuff->primaryFlag;
			entry->attrCatEntry.offset = attrCatBuff->offset;
			
			entry->dirty = true;
			
			return SUCCESS;
		}
	}
	
	return E_ATTRNOTEXIST;
}


/* Converts a attribute catalog record to AttrCatEntry struct
    We get the record as Attribute[] from the BlockBuffer.getRecord() function.
    This function will convert that to a struct AttrCatEntry type.
*/
void AttrCacheTable::recordToAttrCatEntry(union Attribute record[ATTRCAT_NO_ATTRS],AttrCatEntry* attrCatEntry) {
	strcpy(attrCatEntry->relName, record[ATTRCAT_REL_NAME_INDEX].sVal);
	strcpy(attrCatEntry->attrName, record[ATTRCAT_ATTR_NAME_INDEX].sVal);
	attrCatEntry->attrType = (int)record[ATTRCAT_ATTR_TYPE_INDEX].nVal;
	attrCatEntry->primaryFlag = (int)record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;
	attrCatEntry->rootBlock = (int)record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
	attrCatEntry->offset = (int)record[ATTRCAT_OFFSET_INDEX].nVal;
	
}

void AttrCacheTable::attrCatEntryToRecord(AttrCatEntry *attrCatEntry, Attribute record[ATTRCAT_NO_ATTRS]) {
	strcpy(record[ATTRCAT_REL_NAME_INDEX].sVal, attrCatEntry->relName);
	strcpy(record[ATTRCAT_ATTR_NAME_INDEX].sVal, attrCatEntry->attrName);
	record[ATTRCAT_ATTR_TYPE_INDEX].nVal = (int)attrCatEntry->attrType;
	record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = attrCatEntry->primaryFlag;
	record[ATTRCAT_ROOT_BLOCK_INDEX].nVal = attrCatEntry->rootBlock;
	record[ATTRCAT_OFFSET_INDEX].nVal = attrCatEntry->offset;
	
}

int AttrCacheTable::getSearchIndex(int relId, int attrOffset, IndexId *searchIndex) {

	if(relId < 0 || relId >=MAX_OPEN) return E_OUTOFBOUND;
	
	if(attrCache[relId] == nullptr) return E_RELNOTOPEN;

	for(AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next) {
	
		if(entry->attrCatEntry.offset == attrOffset) {
			*searchIndex = entry->searchIndex;
			return SUCCESS;
		}
	}
	
	return E_ATTRNOTEXIST;
}

int AttrCacheTable::getSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex) {

	if(relId < 0 || relId >=MAX_OPEN) return E_OUTOFBOUND;
	
	if(attrCache[relId] == nullptr) return E_RELNOTOPEN;

	for(AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next) {
	
		if(strcmp(entry->attrCatEntry.attrName, attrName) == 0)  {
			*searchIndex = entry->searchIndex;
			return SUCCESS;
		}
	}
	
	return E_ATTRNOTEXIST;
}

int AttrCacheTable::setSearchIndex(int relId, int attrOffset, IndexId *searchIndex) {

	if(relId < 0 || relId >=MAX_OPEN) return E_OUTOFBOUND;
	
	if(attrCache[relId] == nullptr) return E_RELNOTOPEN;
	
	for(AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next) {
	
		if(entry->attrCatEntry.offset == attrOffset) {
		
			entry->searchIndex = *searchIndex;
			return SUCCESS;
		}
	}
	
	return E_ATTRNOTEXIST;
}

int AttrCacheTable::setSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex) {

	if(relId < 0 || relId >=MAX_OPEN) return E_OUTOFBOUND;
	
	if(attrCache[relId] == nullptr) return E_RELNOTOPEN;
	
	for(AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next) {
	
		if(strcmp(entry->attrCatEntry.attrName, attrName) == 0) {
		
			entry->searchIndex = *searchIndex;
			return SUCCESS;
		}
	}
	
	return E_ATTRNOTEXIST;
}

int AttrCacheTable::resetSearchIndex(int relId, int attrOffset) {

	IndexId searchIndex;
	
	searchIndex.block = -1;
	searchIndex.index = -1;
	
	int ret = setSearchIndex(relId, attrOffset, &searchIndex);
	return ret;
}

int AttrCacheTable::resetSearchIndex(int relId, char attrName[ATTR_SIZE]) {

	IndexId searchIndex;
	
	searchIndex.block = -1;
	searchIndex.index = -1;
	
	int ret = setSearchIndex(relId, attrName, &searchIndex);
	return ret;
}


