#ifndef __stringh_h
#define __stringh_h

@extern integer () StringHash_Create;
@extern integer (integer hashid) StringHash_Destroy;
@extern integer (integer hashid, string key, string value, integer value_id) StringHash_Set;
@extern string  (integer hashid, string key, integer value_id) StringHash_Get;
@extern integer (integer hashid) StringHash_Length;
@extern string  (integer hashid, integer idx, integer value_id) StringHash_GetIdx;
@extern integer (integer hashid, integer idx, string value, integer value_id) StringHash_SetIdx;

#endif//__stringh_h
