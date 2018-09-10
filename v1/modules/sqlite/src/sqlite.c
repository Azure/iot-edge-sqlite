// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include "azure_c_shared_utility/gballoc.h"
#include "module.h"

#include <parson.h>
#include "sqlite.h"
#include "message.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/lock.h"

#define BUFSIZE 1024
#define MACSTRLEN 17

typedef struct SQLITE_HANDLE_DATA_TAG
{
    sqlite3 *db;
    BROKER_HANDLE broker;
    const char * mac_address;
    SQLITE_SOURCE * sources;
}SQLITE_HANDLE_DATA;

MESSAGE_CONFIG msgConfig;
MAP_HANDLE propertiesMap;
static int result_idx;
static char onlineText[35] = "{\"notice\":\"sqlite module online!\"}";
static char resultKey[BUFSIZE];
static char resultText[BUFSIZE];
static char errorText[BUFSIZE];
JSON_Value *error_root_value;
JSON_Object *error_root_object;
JSON_Value *result_root_value;
JSON_Object *result_root_object;

static bool isValidMac(char* mac)
{
    //format XX:XX:XX:XX:XX:XX
    bool ret = true;
    int len = strlen(mac);

    if (len != MACSTRLEN)
    {
        LogError("invalid mac length: %d", len);
        ret = false;
    }
    else
    {
        for (int mac_char = 0; mac_char < MACSTRLEN; mac_char++)
        {
            if (((mac_char + 1) % 3 == 0))
            {
                if (mac[mac_char] != ':')
                {
                    ret = false;
                    break;
                }
            }
            else
            {
                if (!((mac[mac_char] >= '0' && mac[mac_char] <= '9') || (mac[mac_char] >= 'a' && mac[mac_char] <= 'f') || (mac[mac_char] >= 'A' && mac[mac_char] <= 'F')))
                {
                    ret = false;
                    break;
                }
            }
        }
    }
    return ret;
}
static SQLITE_SOURCE * find_source(const char * source, SQLITE_HANDLE_DATA * handleData)
{
    SQLITE_SOURCE * find = handleData->sources;
    while (find)
    {
        if (strcmp(source, find->id) == 0)
            break;
        find = find->p_next;
    }
    return find;
}
static int callback(void *NotUsed, int argc, char **argv, char **azColName) 
{
    //publish result to iotHub
    int i;
    for (i = 0; i<argc; i++) {
        LogInfo("%s = %s", azColName[i], argv[i] ? argv[i] : "NULL");
        SNPRINTF_S(resultKey, BUFSIZE, "result[%d].%s", result_idx, azColName[i]);
        json_object_dotset_string(result_root_object, resultKey, argv[i] ? argv[i] : "NULL");
    }
    LogInfo("\n"); 
    result_idx++;
    return 0;
}
static void sqlite_source_cleanup(SQLITE_SOURCE * source)
{
    while (source)
    {
        SQLITE_COLUMN * column = source->columns;
        while (column)
        {
            SQLITE_COLUMN * temp_column = column;
            column = column->p_next;
			if (temp_column->name)
                free((void*)temp_column->name);
			if (temp_column->type)
                free((void*)temp_column->type);
            free(temp_column);
        }

        SQLITE_SOURCE * temp_source = source;
        source = source->p_next;
		if (temp_source->id)
            free((void*)temp_source->id);
		if (temp_source->dbPath)
            free((void*)temp_source->dbPath);
		if (temp_source->table)
            free((void*)temp_source->table);
        free(temp_source);
    }
}
static bool addOneColumn(SQLITE_COLUMN * column, JSON_Object * column_obj)
{
    bool result = true;
    const char* name = json_object_get_string(column_obj, "name");
    const char* type = json_object_get_string(column_obj, "type");
    const char* primaryKey = json_object_get_string(column_obj, "primaryKey");
    const char* notNull = json_object_get_string(column_obj, "notNull");

    if (name == NULL)
    {
        /*Codes_SRS_SQLITE_JSON_99_037: [ If the `columns` object does not contain a value named "name" then Sqlite_ParseConfigurationFromJson shall fail and return NULL. ]*/
        LogError("Did not find expected %s configuration", "name");
        result = false;
    }
    else if (type == NULL)
    {
        /*Codes_SRS_SQLITE_JSON_99_038: [ If the `columns` object does not contain a value named "type" then Sqlite_ParseConfigurationFromJson shall fail and return NULL. ]*/
        LogError("Did not find expected %s configuration", "type");
        result = false;
    }
    else if (primaryKey == NULL)
    {
        /*Codes_SRS_SQLITE_JSON_99_039: [ If the `columns` object does not contain a value named "primaryKey" then Sqlite_ParseConfigurationFromJson shall fail and return NULL. ]*/
        LogError("Did not find expected %s configuration", "primaryKey");
        result = false;
    }
    else if (notNull == NULL)
    {
        /*Codes_SRS_SQLITE_JSON_99_039: [ If the `columns` object does not contain a value named "notNull" then Sqlite_ParseConfigurationFromJson shall fail and return NULL. ]*/
        LogError("Did not find expected %s configuration", "notNull");
        result = false;
    }

    if (!result)
    {
        return result;
    }

    /*Codes_SRS_SQLITE_JSON_99_042: [ `Sqlite_ParseConfigurationFromJson` shall use "name", "type", "primaryKey" and "notNull" values as the fields for an SQLITE_COLUMN structure and add this element to the link list. ]*/
    mallocAndStrcpy_s((char **)&(column->name), name);
    mallocAndStrcpy_s((char **)&(column->type), type);
    column->primaryKey = atoi(primaryKey);
    column->notNull = atoi(primaryKey);

    return result;
}
static bool addAllColumns(SQLITE_SOURCE * source, JSON_Array * column_array)
{
    bool ret = true;
    int column_count = json_array_get_count(column_array);
    int column_idx;
    for (column_idx = 0; column_idx < column_count; column_idx++)
    {
        SQLITE_COLUMN * column = malloc(sizeof(SQLITE_COLUMN));
        if (column == NULL)
        {
            ret = false;
            break;
        }
        else
        {
            memset(column, 0, sizeof(SQLITE_COLUMN));
            column->p_next = source->columns;
            source->columns = column;
            JSON_Object* column_obj = json_array_get_object(column_array, column_idx);
            if (!addOneColumn(column, column_obj))
            {
                ret = false;
                break;
            }
        }
    }
    return ret;
}
static bool addOneSource(SQLITE_SOURCE * source, JSON_Object * source_obj)
{
    bool result = true;
    const char* id = json_object_get_string(source_obj, "id");
    const char* dbPath = json_object_get_string(source_obj, "dbPath");
    const char* table = json_object_get_string(source_obj, "table");
    const char* limit = json_object_get_string(source_obj, "limit");

    if (id == NULL)
    {
        /*Codes_SRS_SQLITE_JSON_99_037: [ If the `sources` object does not contain a value named "id" then Sqlite_ParseConfigurationFromJson shall fail and return NULL. ]*/
        LogError("Did not find expected %s configuration", "id");
        result = false;
    }
    else if (dbPath == NULL)
    {
        /*Codes_SRS_SQLITE_JSON_99_038: [ If the `sources` object does not contain a value named "dbPath" then Sqlite_ParseConfigurationFromJson shall fail and return NULL. ]*/
        LogError("Did not find expected %s configuration", "dbPath");
        result = false;
    }
    else if (table == NULL)
    {
        /*Codes_SRS_SQLITE_JSON_99_039: [ If the `sources` object does not contain a value named "table" then Sqlite_ParseConfigurationFromJson shall fail and return NULL. ]*/
        LogError("Did not find expected %s configuration", "table");
        result = false;
    }
    else if (limit == NULL)
    {
        /*Codes_SRS_SQLITE_JSON_99_039: [ If the `sources` object does not contain a value named "limit" then Sqlite_ParseConfigurationFromJson shall fail and return NULL. ]*/
        LogError("Did not find expected %s configuration", "limit");
        result = false;
    }

    if (!result)
    {
        return result;
    }

    /*Codes_SRS_SQILTE_JSON_99_042: [ `Sqlite_ParseConfigurationFromJson` shall use "id", "dbPath", "table" and "limit" values as the fields for an SQLITE_SOURCE structure and add this element to the link list. ]*/
    mallocAndStrcpy_s((char **)&(source->id), id);
    mallocAndStrcpy_s((char **)&(source->dbPath), dbPath);
    mallocAndStrcpy_s((char **)&(source->table), table);
    source->limit = atoi(limit);

    return result;
}
static bool addAllSources(SQLITE_CONFIG * config, JSON_Array * source_array)
{
    bool ret = true;
    int source_count = json_array_get_count(source_array);
    int source_idx;
    for (source_idx = 0; source_idx < source_count; source_idx++)
    {
        SQLITE_SOURCE * source = malloc(sizeof(SQLITE_SOURCE));
        if (source == NULL)
        {
            ret = false;
            break;
        }
        else
        {
            memset(source, 0, sizeof(SQLITE_SOURCE));
            source->p_next = config->sources;
            config->sources = source;
            JSON_Object* source_obj = json_array_get_object(source_array, source_idx);
            if (!addOneSource(source, source_obj))
            {
                ret = false;
                break;
            }
            JSON_Array * column_array = json_object_get_array(source_obj, "columns"); 
            if (column_array != NULL)
            {
                if (!addAllColumns(source, column_array))
                {
                    ret = false;
                    break;
                }
            }
            else 
            {
                ret = false;
                break;

            }
        }
    }
    return ret;
}
static void sqlite_publish(BROKER_HANDLE broker, SQLITE_HANDLE_DATA * handle)
{
    MESSAGE_HANDLE sqliteMessage = Message_Create(&msgConfig);
    if (sqliteMessage == NULL)
    {
        LogError("unable to create \"sqlite\" message");
    }
    else
    {
        (void)Broker_Publish(broker, handle, sqliteMessage);
        Message_Destroy(sqliteMessage);
    }
}
static void sqlite_exec(SQLITE_HANDLE_DATA* handle, char* sql, int publish)
{
    char *zErrMsg = 0;
    int rc;
    error_root_value = json_value_init_object();
    error_root_object = json_value_get_object(error_root_value);
    result_root_value = json_value_init_object();
    result_root_object = json_value_get_object(result_root_value);
    char *serialized_string = NULL;

    /* Execute SQL statement */
    if (handle != NULL && sql != NULL)
    {
        result_idx = 0;
        
        rc = sqlite3_exec(handle->db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) 
        {
            LogError("SQL error: %s", zErrMsg);    
            json_object_set_string(error_root_object, "error", zErrMsg);

            serialized_string = json_serialize_to_string_pretty(error_root_value);
            msgConfig.source = (const unsigned char *)serialized_string;
            msgConfig.size = strlen(serialized_string);
            if (publish == 1)
            {
                sqlite_publish(handle->broker, handle);
            }

            sqlite3_free(zErrMsg);    
        }
        else 
        {
            LogInfo("operation done successfully");

            serialized_string = json_serialize_to_string_pretty(result_root_value);
            msgConfig.source = (const unsigned char *)serialized_string;
            msgConfig.size = strlen(serialized_string);
            if (publish == 1)
            {
                sqlite_publish(handle->broker, handle);
            }
        }
        json_free_serialized_string(serialized_string);
    }
    json_value_free(error_root_value);
    json_value_free(result_root_value);
}
//select * from sqlite_master where type = 'trigger'; list all triggers
static void sqlite_try_update_trigger(SQLITE_HANDLE_DATA * handleData, SQLITE_SOURCE * src_table)
{
    char sql_trigger[BUFSIZE] = { 0 };
    char sql_drop_trigger[BUFSIZE] = { 0 };
    char sql_primary[BUFSIZE] = { 0 };
    int index;

    SQLITE_COLUMN *column = src_table->columns;
    while (column)
    {
        if (column->primaryKey == 1)
        {
            index = strlen(sql_primary);
            SNPRINTF_S(sql_primary + index, BUFSIZE - index, "%s,", column->name);
        }
        column = column->p_next;
    }
    sql_primary[strlen(sql_primary)-1] = '\0'; // remove last comma

	SNPRINTF_S(sql_drop_trigger, BUFSIZE, "DROP TRIGGER %s_size_control;", src_table->table);
	SNPRINTF_S(sql_trigger, BUFSIZE, "CREATE TRIGGER %s_size_control INSERT ON %s WHEN (SELECT count(*) from %s)>%d\n"
		"BEGIN\n"
		"DELETE FROM %s WHERE rowid <= (SELECT max(rowid) - %d FROM %s);\n"
		"END;",
        src_table->table, src_table->table, src_table->table, src_table->limit,
        src_table->table, src_table->limit, src_table->table
        );
    sqlite_exec(handleData, sql_drop_trigger, 0);
    sqlite_exec(handleData, sql_trigger, 0);
}
//PRAGMA table_info('TABLENAME'); list all columns of 'TABLENAME'
static void sqlite_try_create_table(SQLITE_HANDLE_DATA * handleData, SQLITE_SOURCE * src_table)
{
    char sql_create[BUFSIZE] = { 0 };
    char sql_primary[BUFSIZE] = { 0 };
    int index = 0;
    SQLITE_COLUMN *column = src_table->columns;

    SNPRINTF_S(sql_create + index, BUFSIZE - index, "create table if not exists %s (", src_table->table);
    SNPRINTF_S(sql_primary + index, BUFSIZE - index, "PRIMARY KEY (");
    while (column)
    {
        //construct primary key
        if (column->primaryKey == 1)
        {
            index = strlen(sql_primary);
            SNPRINTF_S(sql_primary + index, BUFSIZE - index, "%s,", column->name);
        }

        //construct create table
        index = strlen(sql_create);
        SNPRINTF_S(sql_create + index, BUFSIZE - index, "%s %s %s,",
            column->name,
            column->type,
            (column->notNull == 1) ? "NOT NULL" : ""
        );
        column = column->p_next;
    }
    index = strlen(sql_primary);
    SNPRINTF_S(sql_primary + index - 1, BUFSIZE - index + 1, ")");

    index = strlen(sql_create);
    SNPRINTF_S(sql_create + index, BUFSIZE - index, "%s);", sql_primary);

    sqlite_exec(handleData, sql_create, 0);
}
static bool sqlite_try_open_db(const char * database, SQLITE_HANDLE_DATA * handleData)
{
    const char * current_db;
    int rc;
    bool ret = false;
    if (handleData->db != NULL)
    {
        current_db = sqlite3_db_filename(handleData->db, "main");
        if (strcmp(current_db, database) != 0)
        {
            sqlite3_close(handleData->db);
            handleData->db = NULL;
        }
        else
            ret = true;
    }
    if (handleData->db == NULL)
    {
        rc = sqlite3_open(database, &(handleData->db));
        if (rc != 0)
        {
            LogError("Can't open database: %s", sqlite3_errmsg(handleData->db));
        }
        else
        {
            LogInfo("Opened database %s successfully", database);
            ret = true;
        }
    }
    return ret;
}
static MODULE_HANDLE Sqlite_Create(BROKER_HANDLE broker, const void* configuration)
{
    bool isValidConfig = true;
    SQLITE_HANDLE_DATA* result = NULL;
    char *zErrMsg = 0;
    SQLITE_CONFIG* config = (SQLITE_CONFIG*)configuration;
    if ((broker == NULL) || (config == NULL))
    {

        /*Codes_SRS_SQLITE_99_001: [If broker is NULL then Sqlite_Create shall fail and return NULL.]*/
        /*Codes_SRS_SQLITE_99_002 : [If configuration is NULL then Sqlite_Create shall fail and return NULL.]*/
        LogError("invalid arg broker=%p configuration=%p", broker, config);
    }
    else
    {
        // get library path from config
        result = malloc(sizeof(SQLITE_HANDLE_DATA));
        if (result != NULL)
        {
            char * mac;
            int copy_result = mallocAndStrcpy_s(&mac, config->mac_address);
            if (copy_result != 0)
            {
                /*Codes_SRS_SQLITE_99_003: [ If any system call fails, Sqlite_Create shall fail and return NULL. ]*/
                LogError("Creating module handle from config failed, error = %d", copy_result);
                free(result);
                result = NULL;
            }
            else
            {
                result->mac_address = mac;
                result->broker = broker;
                result->sources = config->sources;
                result->db = NULL;
            }
        }
    }
    return result;
}
static void Sqlite_Start(MODULE_HANDLE module)
{
    SQLITE_HANDLE_DATA* handleData = module;

    LogInfo("connecting device...");
    propertiesMap = Map_Create(NULL);

    if (handleData != NULL)
    {
        if (propertiesMap == NULL)
        {
            LogError("unable to create a Map");
        }
        else
        {
            if (Map_AddOrUpdate(propertiesMap, "source", "sqlite") != MAP_OK)
            {
                LogError("Could not attach source property to message");
            }
            else if (Map_AddOrUpdate(propertiesMap, "macAddress", handleData->mac_address) != MAP_OK)
            {
                LogError("Could not attach macAddress property to message");
            }
            else
            {
                msgConfig.sourceProperties = propertiesMap;
                msgConfig.source = (const unsigned char *)onlineText;
                msgConfig.size = strlen(onlineText);
                sqlite_publish(handleData->broker, handleData);

                SQLITE_SOURCE * find = handleData->sources;
                while (find)
                {
                    if (sqlite_try_open_db(find->dbPath, handleData))
                    {
                        sqlite_try_create_table(handleData, find);
                    }
                    if (find->limit > 0)
                    {
                        sqlite_try_update_trigger(handleData, find);
                    }
                    find = find->p_next;
                }
            }
        }
    }
}

static void Sqlite_Destroy(MODULE_HANDLE module)
{
    /*Codes_SRS_SQLITE_99_014: [If moduleHandle is NULL then Sqlite_Destroy shall return.]*/
    if (module != NULL)
    {
        SQLITE_HANDLE_DATA* handleData = (SQLITE_HANDLE_DATA*)module;
        if (handleData->db != NULL)
            sqlite3_close(handleData->db);
        if (handleData->mac_address != NULL)
            free((char*)handleData->mac_address);
        sqlite_source_cleanup(handleData->sources);
		if (propertiesMap)
			Map_Destroy(propertiesMap);
        free(handleData);
    }
}

/*
{"dbPath": "D:\\sqlite\\tools\\test.db", "sqlCommand": "select * from COMPANY;"} *** from IoTHub
{"sqlCommand": "upsert to COMPANY;"} *** from other modules
*/

static void Sqlite_Receive(MODULE_HANDLE moduleHandle, MESSAGE_HANDLE messageHandle)
{
    if (moduleHandle == NULL || messageHandle == NULL)
    {
        /*Codes_SRS_SQLITE_99_009: [If moduleHandle is NULL then Sqlite_Receive shall fail and return.]*/
        /*Codes_SRS_SQLITE_99_010 : [If messageHandle is NULL then Sqlite_Receive shall fail and return.]*/
        LogError("Received NULL arguments: module = %p, massage = %p", moduleHandle, messageHandle);
    }
    else
    {
        SQLITE_HANDLE_DATA* handleData = moduleHandle;
        CONSTMAP_HANDLE properties = Message_GetProperties(messageHandle);

        /*Codes_SRS_SQLITE_99_011: [If `messageHandle` properties does not contain a "source" property, then Sqlite_Receive shall fail and return.]*/
        /*Codes_SRS_SQLITE_99_012 : [If `messageHandle` properties contains a "deviceKey" property, then Sqlite_Receive shall fail and return.]*/
        /*Codes_SRS_SQLITE_99_013 : [If `messageHandle` properties contains a "source" property that is set to "mapping", then Sqlite_Receive shall fail and return.]*/
        const char * source = ConstMap_GetValue(properties, "source");
        const char * sqlite_source = ConstMap_GetValue(properties, "sqlite");
        if (source != NULL)
        {
            if (strcmp(source, "mapping") == 0 && !ConstMap_ContainsKey(properties, "deviceKey")) //from IoTHub
            {
                const CONSTBUFFER * content = Message_GetContent(messageHandle); /*by contract, this is never NULL*/
                JSON_Value* json = json_parse_string((const char*)content->buffer);
                if (json == NULL)
                {
                    /*Codes_SRS_SQLITE_99_018 : [If the content of messageHandle is not a JSON value, then `Sqlite_Receive` shall fail and return NULL.]*/
                    LogError("unable to json_parse_string");
                }
                else
                {
                    JSON_Object * obj = json_value_get_object(json);
                    if (obj == NULL)
                    {
                        LogError("json_value_get_obj failed");
                    }
                    else
                    {
                        const char * database = json_object_get_string(obj, "dbPath");
                        const char * sqlcmd = json_object_get_string(obj, "sqlCommand");
                        if (database == NULL)
                        {
                            LogError("database is NULL");
                        }
                        else
                        {
                            if (sqlite_try_open_db(database, handleData))
                            {
                                sqlite_exec(handleData, (char *)sqlcmd, 1);
                            }
                        }
                    }
                    json_value_free(json);
                }
            }
        }
        else if (sqlite_source != NULL)// from other modules
        {
            SQLITE_SOURCE * match_source = NULL;
            match_source = find_source(sqlite_source, handleData);
            if (match_source)
            {
                if (sqlite_try_open_db(match_source->dbPath, handleData))
                {
                    const CONSTBUFFER * content = Message_GetContent(messageHandle); /*by contract, this is never NULL*/
                    JSON_Value* json = json_parse_string((const char*)content->buffer);
                    if (json == NULL)
                    {
                        /*Codes_SRS_SQLITE_99_018 : [If the content of messageHandle is not a JSON value, then `Sqlite_Receive` shall fail and return NULL.]*/
                        LogError("unable to json_parse_string");
                    }
                    else
                    {
                        JSON_Object * obj = json_value_get_object(json);
                        if (obj == NULL)
                        {
                            LogError("json_value_get_obj failed");
                        }
                        else
                        {
                            const char * sqlcmd = json_object_get_string(obj, "sqlCommand");
                            if (sqlcmd == NULL)
                            {
                                LogError("sqlcmd is NULL");
                            }
                            else
                            {
                                sqlite_exec(handleData, (char *)sqlcmd, 0);
                            }
                        }
                        json_value_free(json);
                    }
                }
            }
        }
        ConstMap_Destroy(properties);
    }
    /*Codes_SRS_SQLITE_99_017 : [Sqlite_Receive shall return.]*/
}

static void* Sqlite_ParseConfigurationFromJson(const char* configuration)
{
    SQLITE_CONFIG * result = NULL;
    /*Codes_SRS_SQLITE_JSON_99_023: [ If configuration is NULL then Sqlite_ParseConfigurationFromJson shall fail and return NULL. ]*/
    if ((configuration == NULL))
    {
        LogError("NULL parameter detected configuration=%p", configuration);
    }
    else
    {
        /*Codes_SRS_SQLITE_JSON_99_031: [ If configuration is not a JSON object, then Sqlite_ParseConfigurationFromJson shall fail and return NULL. ]*/
        JSON_Value* json = json_parse_string((const char*)configuration);
        if (json == NULL)
        {
            LogError("unable to json_parse_string");
        }
        else
        {
            JSON_Object* obj = json_value_get_object(json);
            if (obj == NULL)
            {
                LogError("unable to json_value_get_object");
                result = NULL;
            }
            else
            {
                const char* macAddress_json = json_object_get_string(obj, "macAddress");
                JSON_Array * source_array = json_object_get_array(obj, "sources");
                if (macAddress_json == NULL || source_array == NULL)
                {
                    /*Codes_SRS_SQLITE_99_003: [ If any system call fails, Sqlite_ParseConfigurationFromJson shall fail and return NULL. ]*/
                    LogError("json get config failed");
                    result = NULL;
                }
                else
                {
                    result = malloc(sizeof(SQLITE_CONFIG)); 
                    if (result == NULL)
                    {
                        /*Codes_SRS_SQLITE_99_003: [ If any system call fails, Sqlite_ParseConfigurationFromJson shall fail and return NULL. ]*/
                        LogError("malloc failed");
                    }
                    else
                    {
                        memset(result, 0, sizeof(SQLITE_CONFIG));
                        char * macAddress;
                        int copy_result = mallocAndStrcpy_s(&macAddress, macAddress_json);
                        if (copy_result != 0)
                        {
                            /*Codes_SRS_SQLITE_99_003: [ If any system call fails, Sqlite_ParseConfigurationFromJson shall fail and return NULL. ]*/
                            LogError("Copying config from JSON failed, error= %d", copy_result);
                            free(result);
                            result = NULL;
                        }
                        else
                        {
                            /*Codes_SRS_SQLITE_99_006: [ Sqlite_ParseConfigurationFromJson shall return a pointer to the created SQLITE_CONFIG structure. ]*/
                            /**
                            * Everything's good.
                            */
                            result->mac_address = (const char *)macAddress;
                            bool parse_fail = false;
                            if (!addAllSources(result, source_array))
                            {
                                sqlite_source_cleanup(result->sources);
                                free(result);
                                result = NULL;
                            }
                        }
                    }
                }
            }
        }
        json_value_free(json);
    }
    return result;
}

static void Sqlite_FreeConfiguration(void* configuration)
{
    /*Codes_SRS_SQLITE_99_006: [ Sqlite_FreeConfiguration shall only clean lib_path and mac_addess, the rest are done in Sqlite_Destroy. ]*/
    if (configuration != NULL)
    {
        SQLITE_CONFIG* config = (SQLITE_CONFIG*)configuration;
        if (config->mac_address != NULL)
        {
            free((char*)config->mac_address);
        }
        free(config);
    }
}
static const MODULE_API_1 moduleInterface = 
{
    {MODULE_API_VERSION_1},

    Sqlite_ParseConfigurationFromJson,
    Sqlite_FreeConfiguration,
    Sqlite_Create,
    Sqlite_Destroy,
    Sqlite_Receive,
    Sqlite_Start
};

#ifdef BUILD_MODULE_TYPE_STATIC
MODULE_EXPORT const MODULE_API* MODULE_STATIC_GETAPI(SQLITE_MODULE)(const MODULE_API_VERSION gateway_api_version)
#else
MODULE_EXPORT const MODULE_API* Module_GetApi(const MODULE_API_VERSION gateway_api_version)
#endif
{
    (void)gateway_api_version;
    /* Codes_SRS_SQLITE_99_016: [`Module_GetApi` shall return a pointer to a `MODULE_API` structure with the required function pointers.]*/
    return (const MODULE_API *)&moduleInterface;
}
