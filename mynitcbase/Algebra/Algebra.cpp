#include "Algebra.h"

#include <cstring>
#include<bits/stdc++.h>
using namespace std;
/* used to select all the records that satisfy a condition.
the arguments of the function are
- srcRel - the source relation we want to select from
- targetRel - the relation we want to select into. (ignore for now)
- attr - the attribute that the condition is checking
- op - the operator of the condition
- strVal - the value that we want to compare against (represented as a string)
*/

int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE],int op, char strVal[ATTR_SIZE]) {
	int srcRelId = OpenRelTable::getRelId(srcRel);
	if (srcRelId == E_RELNOTOPEN) {
		return E_RELNOTOPEN;
	}
	
	AttrCatEntry attrCatEntry;
	int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
	if(ret == E_ATTRNOTEXIST) {
		return ret;
	}
	

  	/*** Convert strVal (string) to an attribute of data type NUMBER or STRING ***/
  	int type = attrCatEntry.attrType;
  	Attribute attrVal;
  	if(type == NUMBER) {
  	
  		if(isNumber(strVal)) {
  		
  			attrVal.nVal = atof(strVal);
  		}
  		
  		else {
  			return E_ATTRTYPEMISMATCH;
  		}
  	}
  	else if(type == STRING) {
  	
  		strcpy(attrVal.sVal, strVal);
  	}
  	
  	/******* Creating and opening the target relation *******/
  	//getRelCatEntry and no_attrs of src rel
  	RelCatEntry srcRelCatEntry;
  	RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
  	int src_nAttrs = srcRelCatEntry.numAttrs;
  	
  	char attr_names[src_nAttrs][ATTR_SIZE];
  	int attr_types[src_nAttrs];
  	
  	//iterate through 0 to src-nAttrs - 1:
  	//	get the ith attributes attrcatentry to fill attr_name and type 
  	for(int i = 0; i < src_nAttrs; i++) {
  		
  		AttrCatEntry attrCatEntry;
  		AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
  		strcpy(attr_names[i], attrCatEntry.attrName);
  		attr_types[i] = attrCatEntry.attrType;
  	}
  	
  	//Create the relation for target relation by calling Schema::createRel by providing appropriate arguments
  	//if it returns error code return the error value
  	ret = Schema::createRel(targetRel, src_nAttrs, attr_names, attr_types);
  	if(ret != SUCCESS) return ret;
  	
  	//Open the newly created target relation by calling openRel and store targetRelId
  	int targetRelId = OpenRelTable::openRel(targetRel);
  	//if opening fails delete the target relation by calling deleteRel and return targetRelId
  	if(targetRelId < 0 || targetRelId >= MAX_OPEN) {
  		Schema::deleteRel(targetRel);
  		return targetRelId;
  	}
  	
  	/*** Selecting and inserting records into the target relation ***/
  	RelCacheTable::resetSearchIndex(srcRelId);
  	AttrCacheTable::resetSearchIndex(srcRelId, attr);
  	
  	Attribute record[src_nAttrs];
  	
  	
  	//read every record that satisfies the condition by repeatedly calling BA::LinearSearch
  	while(BlockAccess::search(srcRelId, record, attr, attrVal, op) == SUCCESS) {
  		
  		ret = BlockAccess::insert(targetRelId, record);

  		//std::cout<<record[0].nVal<<std::endl;
  		
  		if(ret != SUCCESS) {
  			//close target rel and then delete target rel return rel
  			Schema::closeRel(targetRel);
  			Schema::deleteRel(targetRel);
  			return ret;
  		}
  	}
  	
  	//close targetRel
  	Schema::closeRel(targetRel);
  	
  	return SUCCESS;

}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]) {
	
	int srcRelId = OpenRelTable::getRelId(srcRel);
	
	if(srcRelId == E_RELNOTOPEN) return E_RELNOTOPEN;
	
	//get RelCatEntry of srcRelId
	RelCatEntry srcRelCatEntry;
	RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
	
	int numAttrs = srcRelCatEntry.numAttrs;
	
	char attrNames[numAttrs][ATTR_SIZE];
	int attrTypes[numAttrs];
	
	//iterate through every attribute source of the relation
	for(int i = 0; i < numAttrs; i++) {
		AttrCatEntry attrCatEntry;
		AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
		
		strcpy(attrNames[i], attrCatEntry.attrName);
		attrTypes[i] = attrCatEntry.attrType;
	}
	
	/*** Creating and Opening the target relation ***/
	
	//create relation for target using createRel
	int ret = Schema::createRel(targetRel, numAttrs, attrNames, attrTypes);
	
	if(ret != SUCCESS) return ret;
	
	//open the newly created rel
	int targetRelId = OpenRelTable::openRel(targetRel);
	
	//if opening fails delete Rel
	if(targetRelId < 0 || targetRelId >= MAX_OPEN) {
		Schema::deleteRel(targetRel);
		return targetRelId;
	}
	
	/*** Insert projected records into the target relation ***/
	
	RelCacheTable::resetSearchIndex(srcRelId);
	
	Attribute record[numAttrs];
	
	while(BlockAccess::project(srcRelId, record) == SUCCESS) {
	
		//record will contain next record
		ret = BlockAccess::insert(targetRelId, record);
		
		//if insert fails close and delete rel
		if(ret != SUCCESS) {
			Schema::closeRel(targetRel);
			Schema::deleteRel(targetRel);
			return ret;			
		}
	}
	
	Schema::closeRel(targetRel);
	
	return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]) {

	int srcRelId = OpenRelTable::getRelId(srcRel);
	
	if(srcRelId == E_RELNOTOPEN) return E_RELNOTOPEN;
	
	//get relcatEntry and get num of entries
	RelCatEntry srcRelCatEntry;
	RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
	
	int src_nAttrs = srcRelCatEntry.numAttrs;
	
	int attr_offset[tar_nAttrs];
	int attr_types[tar_nAttrs];
	
	/*** Checking if attributes of target are present in the source relation and storing its offset and types ***/
	for(int i=0; i < tar_nAttrs; i++) {
		AttrCatEntry attrCatEntry;
		AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[i], &attrCatEntry);
		
		attr_offset[i] = attrCatEntry.offset;
		attr_types[i] = attrCatEntry.attrType;
	}
	
	/*** Creating and Opening the target Relation ***/
	int ret = Schema::createRel(targetRel, tar_nAttrs, tar_Attrs, attr_types);
	
	if(ret != SUCCESS) return ret;
	
	int targetRelId = OpenRelTable::openRel(targetRel);
	
	//if opening fails delete target rel
	if(targetRelId < 0 || targetRelId >=MAX_OPEN) {
		Schema::deleteRel(targetRel);
		return targetRelId;
	}
	
	/*** Inserting projected records into the target relation ***/
	
	RelCacheTable::resetSearchIndex(srcRelId);
	
	Attribute record[src_nAttrs];
	
	while(BlockAccess::project(srcRelId, record) == SUCCESS){
	
		//record will contain next record
		Attribute proj_record[tar_nAttrs];
		
		for(int i=0; i<tar_nAttrs; i++) {
		
			proj_record[i] = record[attr_offset[i]];
		}
			
		ret = BlockAccess::insert(targetRelId, proj_record);
			
		if(ret != SUCCESS) {
			Schema::closeRel(targetRel);
			Schema::deleteRel(targetRel);
			return ret;
		}
	}
	
	Schema::closeRel(targetRel);
	
	return SUCCESS;
}

int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE]) {

	if(strcmp(relName,RELCAT_RELNAME)==0 || strcmp(relName,ATTRCAT_RELNAME) ==0 ) {
		return E_NOTPERMITTED;
	}
	
	//get the relation's rel-id using OpenRelTable::getRelId()
	int relId = OpenRelTable::getRelId(relName);
	
	if(relId == E_RELNOTOPEN) {
		return E_RELNOTOPEN;
	}
	
	//get the relation catelog entry from relation cache
	
	RelCatEntry relCatEntry;
	RelCacheTable::getRelCatEntry(relId, &relCatEntry);
	
	if(relCatEntry.numAttrs != nAttrs) {
		return E_NATTRMISMATCH;
	}
	
	Attribute recordValues[nAttrs];
	
	//Converting 2D char array of record values into Attribute array recorValues
	
	for(int i=0;i<nAttrs;i++) {
	
		AttrCatEntry attrCatEntry;
		AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);
		
		int type = attrCatEntry.attrType;
		
		if(type == NUMBER) {
		
			if(isNumber(record[i])) {
			
				recordValues[i].nVal = atof(record[i]);
			
			}
			else{ 
				return E_ATTRTYPEMISMATCH;
			}
		
		}
		
		else if(type == STRING) {
			strcpy(recordValues[i].sVal,record[i]);
		}
	
	}
	
	int retVal = BlockAccess::insert(relId, recordValues);
		
	return retVal;

}

int Algebra::join(char srcRelation1[ATTR_SIZE], char srcRelation2[ATTR_SIZE], char targetRelation[ATTR_SIZE], char attribute1[ATTR_SIZE], char attribute2[ATTR_SIZE]) {

	//get relIds of both
	int srcRelId1,srcRelId2;
	
	srcRelId1 = OpenRelTable::getRelId(srcRelation1);
	srcRelId2 = OpenRelTable::getRelId(srcRelation2);
	
	if(srcRelId1 == E_RELNOTOPEN || srcRelId2 == E_RELNOTOPEN) return E_RELNOTOPEN;
	
	AttrCatEntry attrCatEntry1, attrCatEntry2;
	int ret = AttrCacheTable::getAttrCatEntry(srcRelId1, attribute1, &attrCatEntry1);
	
	if(ret != SUCCESS) return ret;
	
	ret = AttrCacheTable::getAttrCatEntry(srcRelId2, attribute2, &attrCatEntry2);
	
	if(ret != SUCCESS) return ret;
	
	if(attrCatEntry1.attrType != attrCatEntry2.attrType) 
		return E_ATTRTYPEMISMATCH;
	
	//iterate through all the attributes in both source relations and check if there are any other pair of attributes other than join attributes with duplicate names in srcRelation1 and 2 
	
	RelCatEntry relCatEntry1, relCatEntry2;
	RelCacheTable::getRelCatEntry(srcRelId1, &relCatEntry1);
	RelCacheTable::getRelCatEntry(srcRelId2, &relCatEntry2);
	
	int numOfAttributes1 = relCatEntry1.numAttrs;
	int numOfAttributes2 = relCatEntry2.numAttrs;
	for(int i=0; i<numOfAttributes1; i++) {
	
		AttrCatEntry attrCatTemp1;
		AttrCacheTable::getAttrCatEntry(srcRelId1, i, &attrCatTemp1);
		
		if(strcmp(attrCatTemp1.attrName, attribute1) == 0) continue;
		
		for(int j=0; j<numOfAttributes2; j++) {
		
			AttrCatEntry attrCatTemp2;
			AttrCacheTable::getAttrCatEntry(srcRelId2, j, &attrCatTemp2);
			
			if(strcmp(attrCatEntry2.attrName, attribute2) == 0)
				continue;
			if(strcmp(attrCatTemp1.attrName, attrCatTemp2.attrName) == 0)
				return E_DUPLICATEATTR;
		}
	}
	
	//if rel2 doesnt have index on attr2 create one
	if(attrCatEntry2.rootBlock == -1) {
		
		ret = BPlusTree::bPlusCreate(srcRelId2, attribute2);
		if(ret == E_DISKFULL) return E_DISKFULL;
	}
	
	int numOfAttributesInTarget = numOfAttributes1 + numOfAttributes2 - 1;
	
	char targetRelAttrNames[numOfAttributesInTarget][ATTR_SIZE];
	int targetRelAttrTypes[numOfAttributesInTarget];
	
	//iterate through all the attributes in both the source rels
	for(int i=0; i<numOfAttributes1; i++) {
		AttrCatEntry attrCatEntry;
		AttrCacheTable::getAttrCatEntry(srcRelId1, i, &attrCatEntry);
		
		strcpy(targetRelAttrNames[i], attrCatEntry.attrName);
		targetRelAttrTypes[i] = attrCatEntry.attrType;
	}
	
	int k = numOfAttributes1;
	for(int j=0; j<numOfAttributes2; j++) {
		AttrCatEntry attrCatEntry;
		AttrCacheTable::getAttrCatEntry(srcRelId2, j, &attrCatEntry);
		
		//cout<<k<<"-"<<j<<" "<<attrCatEntry.attrName<<endl;
		if(j == attrCatEntry2.offset) continue;
		strcpy(targetRelAttrNames[k], attrCatEntry.attrName);
		//cout<<"HI"<<endl;
		targetRelAttrTypes[k] = attrCatEntry.attrType;
		k++;
	}
	
	//create target rel
	ret = Schema::createRel(targetRelation, numOfAttributesInTarget, targetRelAttrNames, targetRelAttrTypes);
	
	if(ret != SUCCESS) return ret;
	
	//open target rel
	int targetRelId = OpenRelTable::openRel(targetRelation);
	
	if(targetRelId < 0 || targetRelId >= MAX_OPEN) {
	
		Schema::deleteRel(targetRelation);
		
		return targetRelId;
	}
	
	Attribute record1[numOfAttributes1];
	Attribute record2[numOfAttributes2];
	Attribute targetRecord[numOfAttributesInTarget];
	
	RelCacheTable::resetSearchIndex(srcRelId1);
	
	while(BlockAccess::project(srcRelId1, record1) == SUCCESS) {
	
		//cout<<record1[0].sVal;
		//reset search index of srcrel2 and attribute2
		RelCacheTable::resetSearchIndex(srcRelId2);
		
		AttrCacheTable::resetSearchIndex(srcRelId2, attribute2);
		
		//this loop is to get every record of srcrel2 which satisfies:-
		//record1.attribute1 = record2.attribute2
		//cout<<" "<<attrCatEntry1.attrName<<endl;
		while(BlockAccess::search(srcRelId2, record2, attribute2, record1[attrCatEntry1.offset], EQ) == SUCCESS) {
		
			//copy values from record1 and record2 to targetRecord
			for(int i=0; i<numOfAttributes1; i++) {
				targetRecord[i] = record1[i];
			}
			
			k=numOfAttributes1;
			
			for(int j=0; j<numOfAttributes2; j++) {
				if(j == attrCatEntry2.offset) continue;
				//cout<<k<<"-"<<endl;
				targetRecord[k] = record2[j];
				k++;
			}
			
			ret = BlockAccess::insert(targetRelId, targetRecord);
			
			if(ret == E_DISKFULL) {
				OpenRelTable::closeRel(targetRelId);
				Schema::deleteRel(targetRelation);
				return E_DISKFULL;
			}
		}
	}
	
	OpenRelTable::closeRel(targetRelId);
	return SUCCESS;
}

bool isNumber(char *str) {
	int len;
	float ignore;
  /*
    sscanf returns the number of elements read, so if there is no float matching
    the first %f, ret will be 0, else it'll be 1

    %n gets the number of characters read. this scanf sequence will read the
    first float ignoring all the whitespace before and after. and the number of
    characters read that far will be stored in len. if len == strlen(str), then
    the string only contains a float with/without whitespace. else, there's other
    characters.
  */
  	int ret = sscanf(str,"%f %n", &ignore, &len);
  	return ret == 1 && len == strlen(str);
	
}
