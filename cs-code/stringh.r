#include "stringh.h"

integer () StringHash_Create = #0;
integer (integer hashid) StringHash_Destroy = #0;
integer (integer hashid, string key, string value, integer value_id) StringHash_Set = #0;
string  (integer hashid, string key, integer value_id) StringHash_Get = #0;
integer (integer hashid) StringHash_Length = #0;
string  (integer hashid, integer idx, integer value_id) StringHash_GetIdx = #0;
integer (integer hashid, integer idx, string value, integer value_id) StringHash_SetIdx = #0;
