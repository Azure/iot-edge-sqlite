# SQLITE MODULE

## Overview

This module is a SQLite manager which executes SQLite commands sent from other modules. The module also return the execution result to IoT Hub.

### Additional data types
```c

struct SQLITE_COLUMN_TAG
{
    SQLITE_COLUMN * p_next;
    const char * name;
    const char * type;
    int primaryKey;
    int notNull;
};

struct SQLITE_SOURCE_TAG
{
    SQLITE_SOURCE * p_next;
    const char * id;
    const char * dbPath;
    const char * table;
    int limit;
    SQLITE_COLUMN * columns;
};

struct SQLITE_CONFIG_TAG
{
    const char * mac_address;
    SQLITE_SOURCE * sources;
};

```
## Sqlite_ParseConfigurationFromJson
```c
static void* Sqlite_ParseConfigurationFromJson(const void* configuration);
```
Creates a new configuration for SQLite module instance from a JSON string.`configuration` is a pointer to a `const void*` that contains a json object as supplied by `Gateway_CreateFromJson`.
By convention the json object should contain the source modules and their target database files and tables.

### Expected Arguments

The arguments to this module is a JSON object with the following information:
```json
    {
        "macAddress": "<mac address in canonical form>",
        "sources": [
          {
            "id": "<id of the source module, this id will be used as filter while receiving commands>",
            "dbPath": "<target db file>",
            "table": "<target table>",
            "limit": "<max number of rows in the table>",
            "columns": [
              {
                "name": "<name of the column>",
                "type": "<type of the column>",
                "primaryKey": "<0/1 to specify the column to be primary key>",
                "notNull": "<0/1 to specify the column can be Null>"
              }
            ]
          }
        ]
      }
    }
```
Example:
The following Gateway config file describes an instance of the "sqlite" module, available .\sqlite.dll:
```json
    {
      "name": "sqlite",
      "loader": {
        "name": "native",
        "entrypoint": {
          "module.path": "sqlite.dll"
        }
      },
      "args": {
        "macAddress": "01:01:01:01:01:01",
        "sources": [
          {
            "id": "modbus",
            "dbPath": "D:\\test.db",
            "table": "MODBUS",
            "limit": "10",
            "columns": [
              {
                "name": "DATETIME",
                "type": "CHAR(19)",
                "primaryKey": "1",
                "notNull": "1"
              },
              {
                "name": "MAC",
                "type": "CHAR(17)",
                "primaryKey": "1",
                "notNull": "1"
              },
              {
                "name": "VALUE",
                "type": "INT",
                "primaryKey": "0",
                "notNull": "1"
              },
              {
                "name": "ADDRESS",
                "type": "INT",
                "primaryKey": "1",
                "notNull": "1"
              }
            ]
          }
        ]
      }
    }
```