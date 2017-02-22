// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef SQLITE_H
#define SQLITE_H

#include "module.h"
#include "sqlite3.h"

#ifdef WIN32
#define SNPRINTF_S sprintf_s
#else
#define SNPRINTF_S snprintf
#endif
#define handleDLL void *

typedef struct SQLITE_COLUMN_TAG SQLITE_COLUMN;
typedef struct SQLITE_SOURCE_TAG SQLITE_SOURCE;
typedef struct SQLITE_CONFIG_TAG SQLITE_CONFIG;

struct SQLITE_COLUMN_TAG
{
    SQLITE_COLUMN * p_next;
    const char * name;
    const char * type;
    //default value
    int primaryKey;
    int notNull;
    //row id
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
}; /*this needs to be passed to the Module_Create function*/

#ifdef __cplusplus
extern "C"
{
#endif

MODULE_EXPORT const MODULE_API* MODULE_STATIC_GETAPI(SQLITE_MODULE)(const MODULE_API_VERSION gateway_api_version);

#ifdef __cplusplus
}
#endif


#endif /*SQLITE_H*/
