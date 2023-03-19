#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

/* Hash table entry struct
*
*   Fields:
*   - ObjString* key: string object containing hash field
*   - Value value
*/
typedef struct {
    ObjString* key; // string object containing hash field
    Value value;
} Entry;

/* Hash table struct
*
*   Fields:
*   - int count: current number of entries
*   - int capacity: current capacity
*   - Entry* entries: pointer to first entry
*/
typedef struct {
    int count; // current number of entries
    int capacity; // current capacity
    Entry* entries; // pointer to first entry
} Table;

/* Initialize hash table
*   Arguments:
*   - Table* table: to be initialized
*/
void initTable(Table* table);

/* Free memory of a hash table
*   Arguments:
*   - Table* table: to be freed
*/
void freeTable(Table* table);

/* Get entry from hash table
*   Arguments:
*   - Table* table: to search in
*   - ObjString* key: to be found
*   - Value* value: pointer to the value that should be updated with the found value
*/
bool tableGet(Table* table, ObjString* key, Value* value);

/* Update or add entry to hash table
*   Arguments:
*   - Table* table: pointer to the table to update
*   - ObjString* key: of the entry to be updated/added
*   - Value value: of the entry to be updated/added
*
*   Return whether new entry was added
*/
bool tableSet(Table* table, ObjString* key, Value value);

/* Delete entry from hash table
*   Arguments:
*   - Table* table: pointer to the table to delete from
*   - ObjString* key: of the entry to be deleted
*
*   Return whether entry was deleted
*/
bool tableDelete(Table* table, ObjString* key);

/* Copy all entries from one table to another
*   Arguments:
*   - Table* from: table to copy
*   - Table* to: table to paste
*/
void tableAddAll(Table* from, Table* to);

/* Get interned string from hash table
*   Arguments:
*   - Table* table: table to look in
*   - const char* chars: of the key we look for
*   - int length: of the key we look for
*   - uint32_t hash: of the key we look for
*/
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

/* Remove entries that cannot longer be accessed. For garbage collecting purposes
*   Arguments:
*   - Table* table: to delete from
*/
void tableRemoveWhite(Table* table);

/* Mark as accessable all enries and values in hash table. For garbage collecting purposes
*   Arguments:
*   - Table* table: to mark
*/
void markTable(Table* table);

#endif