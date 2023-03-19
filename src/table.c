#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75 // Load percentage threshold that when exceeded table growth will be triggered

/* Initialize hash table
*   Arguments:
*   - Table* table: to be initialized
*/
void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

/* Free memory of a hash table
*   Arguments:
*   - Table* table: to be freed
*/
void freeTable(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

/* Find entry within hash table
*   Arguments:
*   - Entry* entries: pointer to the first entry of hash table t search in
*   - int capacity: number of entries within hash table
*   - ObjString* key: to be found
*
*   Return entry if found
*/
static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash & (capacity - 1); //Optimized wrapping around the table
    Entry* tombstone = NULL;

    for (;;) {
        Entry* entry = &entries[index];

        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) { // Truly empty entry found
                // If tombstone was found previously return it so it ca be reused. Return empty entry otherwise.
                return tombstone != NULL ? tombstone : entry;
            } else if (tombstone == NULL) {
                // First tombstone found, storing in <tombstone>
                tombstone = entry;
            }
        } else if (entry->key == key) {
            //Key found
            return entry;
        }

        index = (index + 1) & (capacity - 1); //Optimized wrapping around the table
    }
}

/* Get entry from hash table
*   Arguments:
*   - Table* table: to search in
*   - ObjString* key: to be found
*   - Value* value: pointer to the value that should be updated with the found value
*/
bool tableGet(Table* table, ObjString* key, Value* value) {
    if (table->count ==0) return false; // Nothing to look in

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false; // Did not found the key

    *value = entry->value;
    return true;
}

/* Adjust capicity for table entries
*   Arguments:
*   - Table* table: to resize
*   - int capacity: the new capacity
*/
static void adjustCapacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity); // Allocate memory for the new entries

    // Initialize new entries
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    // As the capicity is used in calulation of the bucket the entries must be reassigned
    table->count = 0;
    for (int i = 0; i < table->capacity; i++) { // For all entries
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue; // Skip over empty entries and tombstones

        Entry* dest = findEntry(entries, capacity, entry->key); // Find to which new bucket entry should be assigned
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity); // Release memory of the old array

    table->entries = entries;
    table->capacity = capacity;
}

/* Update or add entry to hash table
*   Arguments:
*   - Table* table: pointer to the table to update
*   - ObjString* key: of the entry to be updated/added
*   - Value value: of the entry to be updated/added
*
*   Return whether new entry was added
*/
bool tableSet(Table* table, ObjString* key, Value value) {

    // Adjust capacity of the table if capacity would cross TABLE_MAX_LOAD threshold
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }
    Entry* entry = findEntry(table->entries, table->capacity, key); // Find to which bucket entry should be assigned
    bool isNewKey = entry->key == NULL; // Check if entry with that key already existed

    if (isNewKey && IS_NIL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;

    return isNewKey;
}

/* Delete entry from hash table
*   Arguments:
*   - Table* table: pointer to the table to delete from
*   - ObjString* key: of the entry to be deleted
*
*   Return whether entry was deleted
*/
bool tableDelete(Table* table, ObjString* key) {
    if (table->count == 0) return false; // Nothing to delete

    Entry* entry = findEntry(table->entries, table->capacity, key);

    if (entry->key == NULL) return false;

    // Following pair of values indicate value is a tombstone, required for hash collision solving
    entry->key == NULL;
    entry->value = BOOL_VAL(true);

    return true;
}

/* Copy all entries from one table to another
*   Arguments:
*   - Table* from: table to copy
*   - Table* to: table to paste
*/
void tableAddAll(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) { // For all entries
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

/* Get interned string from hash table
*   Arguments:
*   - Table* table: table to look in
*   - const char* chars: of the key we look for
*   - int length: of the key we look for
*   - uint32_t hash: of the key we look for
*/
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL; // Nothing to look in

    uint32_t index = hash & (table->capacity - 1); //Optimized wrapping around the table

    for (;;) {
        Entry* entry = &table->entries[index];

        if (entry->key == NULL) { // Tombstone or empty entry found
            if (IS_NIL(entry->value)) return NULL; // If empty entry then string that we look for does nt exist
        } else if (entry->key->length == length && entry->key->hash == hash 
                    && memcmp(entry->key->chars,chars, length) == 0) { // Found exact entry
                        return entry->key;
                    }
        index = (index + 1) & (table->capacity -1); //Optimized wrapping around the table
    }
}

/* Remove entries that cannot longer be accessed. For garbage collecting purposes
*   Arguments:
*   - Table* table: to delete from
*/
void tableRemoveWhite(Table* table) {
    for (int i = 0; i < table->capacity; i++) { // For all entries
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.isMarked) { // If not marked by GC "mark" stage
            tableDelete(table, entry->key);
        }
    }
}

/* Mark as accessable all enries and values in hash table. For garbage collecting purposes
*   Arguments:
*   - Table* table: to mark
*/
void markTable(Table* table) {
    for (int i = 0; i < table->capacity; i++) { // For all entries
        Entry* entry = &table->entries[i];
        markObject((Obj*)entry->key);
        markValue(entry->value);
    }
}