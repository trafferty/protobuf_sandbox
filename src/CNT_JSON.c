#include <string.h>
#include <stdlib.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <locale>
#include <sys/types.h>
#include <sys/stat.h>

#include "CNT_JSON.h"

bool createStringHash(string inStr, int &hash)
{
    /*
    * This is a classic hash algor, from dan bernstein many years ago in comp.lang.c.
    * See http://www.eternallyconfuzzled.com/tuts/algorithms/jsw_tut_hashing.aspx
    */
    unsigned char *key = (unsigned char*) inStr.c_str();
    size_t len = inStr.length();

    hash = 0;
    for (unsigned int i = 0; i < len; i++)
        hash = 33 * hash ^ key[i];

    return true;
}

RPCObj createRPCObj(string method, int id)
{
    RPCObj RPC_Obj;

    // build base
    RPC_Obj = cJSON_CreateObject();
    cJSON_AddItemToObject(RPC_Obj, "method", cJSON_CreateString(method.c_str()));
    cJSON_AddItemToObject(RPC_Obj, "id", cJSON_CreateNumber(id));

    return RPC_Obj;
}

RPCObj createRPCObj(string method, int id, map<string, string> Args)
{
    RPCObj RPC_Obj;

    RPC_Obj = createRPCObj(method, id);
    addParamsMap(RPC_Obj, Args);

    return RPC_Obj;
}

int getID(RPCObj RPC_Obj)
{
    int id;

    if (getAttributeValue_Int(RPC_Obj, "id", id))
    {
        return id;
    }
    else
    {
        return INVALID_ID;
    }
}

RPC_RespObj createRPC_RespObj(int id)
{
    RPC_RespObj resp_obj = cJSON_CreateObject();
    cJSON_AddItemToObject(resp_obj, "id", cJSON_CreateNumber(id));
    return resp_obj;
}

RPC_RespObj createRPC_RespObj(RPCObj RPC_Obj)
{
    return createRPC_RespObj(getID(RPC_Obj));
}

bool addParamsMap(RPCObj RPC_Obj, map<string, string> Args)
{
    cJSON *args, *tmpObj;
    string attributeVal;

    if (!RPC_Obj)
    {
        cerr << "Invalid RPCObj" << endl;
        return false;
    }

    if (!doesAttributeExist(RPC_Obj, "params"))
    {
        cerr << "Creating \"params\" object" << endl;
        cJSON_AddItemToObject(RPC_Obj, "params", args = cJSON_CreateObject());
    }
    else
    {
        args = cJSON_GetObjectItem(RPC_Obj, "params");
    }

    map<string, string>::iterator iter;
    for (iter = Args.begin(); iter != Args.end(); ++iter)
    {
        tmpObj = cJSON_GetObjectItem(args, iter->first.c_str());
        if (!tmpObj)
            cJSON_AddItemToObject(args, iter->first.c_str(), cJSON_CreateString(
            iter->second.c_str()));
        else
        {
            strcpy(tmpObj->valuestring, iter->second.c_str());
        }
    }

    return true;
}

bool addParamsMap(RPCObj actionObj, map<string, double> Args)
{
    cJSON *args, *tmpObj;
    string attributeVal;

    if (!actionObj)
    {
        cerr << "Invalid RPCObj" << endl;
        return false;
    }

    if (!doesAttributeExist(actionObj, "params"))
    {
        cerr << "Creating \"params\" object" << endl;
        cJSON_AddItemToObject(actionObj, "params", args = cJSON_CreateObject());
    }
    else
    {
        args = cJSON_GetObjectItem(actionObj, "params");
    }

    map<string, double>::iterator iter;
    for (iter = Args.begin(); iter != Args.end(); ++iter)
    {
        tmpObj = cJSON_GetObjectItem(args, iter->first.c_str());
        if (!tmpObj)
            cJSON_AddItemToObject(args, iter->first.c_str(),
            cJSON_CreateNumber(iter->second));
        else
        {
            tmpObj->valuedouble = iter->second;
        }
    }

    return true;
}

bool doesAttributeExist(cJSON* JSONObj, string AttributeKey, bool recurse)
{
    cJSON *subitem = JSONObj;

    // if this is an array or object, move down one level
    if ((cJSON_Array == subitem->type) || (cJSON_Object == subitem->type))
        subitem = subitem->child;

    while (subitem)
    {
        // check the current item first
        if (subitem->string && (strlen(subitem->string) > 0))
            if (AttributeKey.compare(subitem->string) == 0)
                return true;

        // check to see if this is an array or object, and recurse if flag is set
        if (((cJSON_Array == subitem->type) || (cJSON_Object == subitem->type)) && recurse)
        {
            if (doesAttributeExist(subitem, AttributeKey, recurse))
                return true;
            else
            {
                subitem = subitem->next;
                continue;
            }
        }

        subitem = subitem->next;
    }
    return false;
}

bool getAttributeValue_Int(cJSON* JSONObj, string AttributeKey, int &valueint)
{
    cJSON *subitem;

    switch (JSONObj->type)
    {
    case cJSON_Number:
        subitem = JSONObj;
        break;
    case cJSON_Object:
        subitem = JSONObj->child;
        break;
    default:
        subitem = JSONObj->next;
        break;
    }

    while (subitem)
    {
        if ((AttributeKey.compare(subitem->string) == 0) &&
            (cJSON_Number == subitem->type))
        {
            valueint = subitem->valueint;
            return true;
        }
        else
        {
            subitem = subitem->next;
        }
    }

    return false;
}

bool getAttributeValue_IntArray(cJSON* JSONObj, string AttributeKey, int* valueint, unsigned int nValues)
{
    cJSON *subitem;

    switch (JSONObj->type)
    {
    case cJSON_Array:
        subitem = JSONObj;
        break;
    case cJSON_Object:
        subitem = JSONObj->child;
        break;
    default:
        subitem = JSONObj->next;
        break;
    }

    while (subitem)
    {
        if ((AttributeKey.compare(subitem->string) == 0) &&
            (cJSON_Array == subitem->type))
        {
            unsigned int	index = 0;

            while ((index < (unsigned int)cJSON_GetArraySize(subitem)) && (index < nValues))
            {
                valueint[index] = cJSON_GetArrayItem(subitem, index)->valueint;
                index++;
            }
            return true;
        }
        else
        {
            subitem = subitem->next;
        }
    }

    return false;
}

/*
Get an int from a json object, returning a default "missing" value
if the key is not found.
*/
int getAttributeDefault_Int(cJSON* JSONObj, string AttributeKey, int missing)
{
    int temp;
    if (!getAttributeValue_Int(JSONObj, AttributeKey, temp))
    {
        return missing;
    }
    return temp;
}

bool setAttributeValue_Int(cJSON* JSONObj, string AttributeKey, int valueint)
{
    cJSON *subitem;

    switch (JSONObj->type)
    {
    case cJSON_Number:
        subitem = JSONObj;
        break;
    case cJSON_Object:
        subitem = JSONObj->child;
        break;
    default:
        subitem = JSONObj->next;
        break;
    }

    while (subitem)
    {
        if ((AttributeKey.compare(subitem->string) == 0) && (cJSON_Number == subitem->type))
        {
            cJSON_ReplaceItemInObject(JSONObj, subitem->string, cJSON_CreateNumber(valueint));
            subitem->valueint = valueint;
            return true;
        }
        else
        {
            subitem = subitem->next;
        }
    }
    // if we get here, then the attribute doesn't exist, so add it at the top level
    cJSON_AddItemToObject(JSONObj, AttributeKey.c_str(), cJSON_CreateNumber(valueint));

    return true;
}

bool incrAttributeValue_Int(cJSON* JSONObj, string AttributeKey, int incr)
{
    int curVal;
    if (getAttributeValue_Int(JSONObj, AttributeKey, curVal))
        setAttributeValue_Int(JSONObj, AttributeKey, (curVal+incr));
    else
        return false;

    return true;
}

bool getAttributeValue_Bool(cJSON* JSONObj, string AttributeKey, bool &valuebool)
{
    cJSON *subitem;

    switch (JSONObj->type)
    {
    case cJSON_True:
    case cJSON_False:
        subitem = JSONObj;
        break;
    case cJSON_Object:
        subitem = JSONObj->child;
        break;
    default:
        subitem = JSONObj->next;
        break;
    }

    while (subitem)
    {
        if (AttributeKey.compare(subitem->string) == 0)
        {
            if (cJSON_True == subitem->type)
                valuebool = true;
            else if (cJSON_False == subitem->type)
                valuebool = false;
            else
                return false;

            return true;
        }
        else
        {
            subitem = subitem->next;
        }
    }

    return false;
}

bool setAttributeValue_Bool(cJSON* JSONObj, string AttributeKey, bool valuebool)
{
    cJSON *subitem;

    switch (JSONObj->type)
    {
    case cJSON_True:
    case cJSON_False:
        subitem = JSONObj;
        break;
    case cJSON_Object:
        subitem = JSONObj->child;
        break;
    default:
        subitem = JSONObj->next;
        break;
    }

    while (subitem)
    {
        if ((AttributeKey.compare(subitem->string) == 0) && ((cJSON_True == subitem->type) || (cJSON_False == subitem->type)))
        {
            if (valuebool)
                subitem->type =cJSON_True;
            else
                subitem->type =cJSON_False;

            return true;
        }
        else
        {
            subitem = subitem->next;
        }
    }
    // if we get here, then the attribute doesn't exist, so add it at the top level
    if (valuebool)
        cJSON_AddItemToObject(JSONObj, AttributeKey.c_str(), cJSON_CreateTrue());
    else
        cJSON_AddItemToObject(JSONObj, AttributeKey.c_str(), cJSON_CreateFalse());

    return true;
}

/*
Get a double from a json object, returning a default "missing" value
if the key is not found.
*/
double getAttributeDefault_Double(cJSON* JSONObj, string AttributeKey, double missing)
{
    double temp;
    if (!getAttributeValue_Double(JSONObj, AttributeKey, temp))
    {
        return missing;
    }
    return temp;
}

bool getAttributeValue_Double(cJSON* JSONObj, string AttributeKey, double &valuedouble)
{
    cJSON *subitem;

    switch (JSONObj->type)
    {
    case cJSON_Number:
        subitem = JSONObj;
        break;
    case cJSON_Object:
        subitem = JSONObj->child;
        break;
    default:
        subitem = JSONObj->next;
        break;
    }

    while (subitem)
    {
        if ((AttributeKey.compare(subitem->string) == 0) && (cJSON_Number == subitem->type))
        {
            valuedouble = subitem->valuedouble;
            return true;
        }
        else
        {
            subitem = subitem->next;
        }
    }

    return false;
}

bool setAttributeValue_Double(cJSON* JSONObj, string AttributeKey, double valuedouble)
{
    cJSON *subitem;

    switch (JSONObj->type)
    {
    case cJSON_Number:
        subitem = JSONObj;
        break;
    case cJSON_Object:
        subitem = JSONObj->child;
        break;
    default:
        subitem = JSONObj->next;
        break;
    }

    while (subitem)
    {
        if ((AttributeKey.compare(subitem->string) == 0) && (cJSON_Number == subitem->type))
        {
            cJSON_ReplaceItemInObject(JSONObj, subitem->string, cJSON_CreateNumber(valuedouble));
            return true;
        }
        else
        {
            subitem = subitem->next;
        }
    }
    // if we get here, then the attribute doesn't exist, so add it at the top level
    cJSON_AddItemToObject(JSONObj, AttributeKey.c_str(), cJSON_CreateNumber(valuedouble));

    return true;
}

bool incrAttributeValue_Double(cJSON* JSONObj, string AttributeKey, double incr)
{
    double curVal;
    if (getAttributeValue_Double(JSONObj, AttributeKey, curVal))
        setAttributeValue_Double(JSONObj, AttributeKey, (curVal+incr));
    else
        return false;

    return true;
}

bool getAttributeValue_String(cJSON* JSONObj, string AttributeKey,
                              std::string &valuestring)
{
    cJSON *subitem;

    switch (JSONObj->type)
    {
    case cJSON_String:
        subitem = JSONObj;
        break;
    case cJSON_Object:
        subitem = JSONObj->child;
        break;
    default:
        subitem = JSONObj->next;
        break;
    }

    while (subitem)
    {
        if ((AttributeKey.compare(subitem->string) == 0) && (cJSON_String == subitem->type))
        {
            valuestring.assign(subitem->valuestring);
            return true;
        }
        else
        {
            subitem = subitem->next;
        }
    }

    return false;
}

bool setAttributeValue_String(cJSON* JSONObj, string AttributeKey,
                              std::string valuestring)
{
    cJSON *subitem;

    switch (JSONObj->type)
    {
    case cJSON_String:
        subitem = JSONObj;
        break;
    case cJSON_Object:
        subitem = JSONObj->child;
        break;
    default:
        subitem = JSONObj->next;
        break;
    }

    while (subitem)
    {
        if ((AttributeKey.compare(subitem->string) == 0) && (cJSON_String == subitem->type))
        {
            cJSON_ReplaceItemInObject(JSONObj, subitem->string, cJSON_CreateString(valuestring.c_str()));
            return true;
        }
        else
        {
            subitem = subitem->next;
        }
    }

    // if we get here, then the attribute doesn't exist, so add it at the top level
    cJSON_AddItemToObject(JSONObj, AttributeKey.c_str(), cJSON_CreateString(valuestring.c_str()));

    return true;
}

bool MapToJSON_Str_Double(map<string, double> map_double, cJSON **JSONObj)
{
    *JSONObj = cJSON_CreateObject();
    if (!*JSONObj)
        return false;

    std::map<string, double>::iterator statPair;

    for (statPair = map_double.begin(); statPair != map_double.end(); ++statPair)
    {
        cJSON_AddItemToObject(*JSONObj, statPair->first.c_str(), cJSON_CreateNumber(
            statPair->second));
    }

    return true;
}

bool MapToJSON_Int_Double(map<int, double> map_double, cJSON **JSONObj)
{
    std::stringstream ss;

    *JSONObj = cJSON_CreateObject();
    if (!*JSONObj)
        return false;

    std::map<int, double>::iterator statPair;

    for (statPair = map_double.begin(); statPair != map_double.end(); ++statPair)
    {
        ss.clear();
        ss.str("");
        ss << statPair->first;
        cJSON_AddItemToObject(*JSONObj, ss.str().c_str(), cJSON_CreateNumber(
            statPair->second));
    }

    return true;
}

bool JSONToMap_Str_Double(cJSON *JSONObj, map<string, double> *map_double)
{
    cJSON *item;
    std::stringstream ss;
    map<string, double>::iterator findIter;

    if ((!JSONObj) || (!map_double))
        return false;

    item = JSONObj->child;

    while (item)
    {
        // we only want numbers, so filter by type
        if (cJSON_Number != item->type)
        {
            item = item->next;
            continue;
        }

        ss.clear();
        ss.str("");
        ss << item->string;

        findIter = map_double->find(ss.str());

        if (findIter == map_double->end())
        {
            // if the key does not exist, insert as new key/value
            map_double->insert(make_pair(ss.str(), item->valuedouble));
        }
        else
        {
            // if the key does exist, replace the value
            findIter->second = item->valuedouble;
        }
        item = item->next;
    }
    return true;
}

bool JSONToMap_Int_Double(cJSON *JSONObj, map<int, double> *map_double)
{
    cJSON *item;
    int key;
    std::stringstream ss;
    map<int, double>::iterator findIter;

    if ((!JSONObj) || (!map_double))
        return false;

    item = JSONObj->child;
    while (item)
    {
        // we only want numbers, so filter by type
        if (cJSON_Number != item->type)
        {
            item = item->next;
            continue;
        }

        ss.clear();
        ss.str("");
        ss << item->string;
        if ((ss >> key).fail())
        {
            return false;
        }

        findIter = map_double->find(key);

        if (findIter == map_double->end())
        {
            // if the key does not exist, insert as new key/value
            map_double->insert(make_pair(key, item->valuedouble));
        }
        else
        {
            // if the key does exist, replace the value
            findIter->second = item->valuedouble;
        }

        item = item->next;
    }
    return true;
}

bool writeJSONToFile(cJSON *JSONObject, string filename)
{
    ofstream JSONFile;
    char *JSONBuf;
    bool ret(false);
    size_t size;

    JSONBuf = cJSON_Print(JSONObject);
    size = strlen(JSONBuf);

    if (size)
    {
        JSONFile.open(filename.c_str(), ifstream::binary);

        if (JSONFile.is_open())
        {
            if (JSONFile.write(JSONBuf, size).bad())
                ret = false;
            else
                ret = true;

            free(JSONBuf);
            JSONFile.close();
        }
    }

    return ret;
}

bool readJSONFromFile(cJSON **JSONObject, string filename)
{
    ifstream JSONFile;
    struct stat buf;
    char *JSONBuf;
    bool ret(false);
    std::streamoff size;

#ifdef WIN64
    //DWORD dwAttrib = GetFileAttributes(filename.c_str());
    //if ((dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) == false)
    //{
    //    return ret;
    //}
    if ((stat(filename.c_str(), &buf) == -1))
    {
        return ret;
    }
#else
    if ((stat(filename.c_str(), &buf) == -1) || (!(buf.st_mode & S_IROTH) && !(buf.st_mode
        & S_IRGRP) && !(buf.st_mode & S_IRUSR)))
    {
        return ret;
    }
#endif

    JSONFile.open(filename.c_str());

    if (JSONFile.is_open(), ifstream::binary)
    {
        // get size of file
        JSONFile.seekg(0, ifstream::end);
        size = JSONFile.tellg();
        JSONFile.seekg(0, ifstream::beg);

        // allocate memory for file content
        JSONBuf = new char[size];
        memset(JSONBuf, 0, size);

        // read content of infile
        JSONFile.read(JSONBuf, size);

        *JSONObject = cJSON_Parse(JSONBuf);

        // release dynamically-allocated memory
        delete[] JSONBuf;

        JSONFile.close();

        if (*JSONObject)
        {
            ret = true;
        }
        else
        {
            printf("cJSON - Error before: [%s]\n", cJSON_GetErrorPtr());
            ret = false;
        }
    }
    else
    {
        ret = false;
    }

    return ret;
}

bool mergeJSONObjects(cJSON *targetJSONObj, cJSON *sourceJSONObj)
{
    bool ret(false);
    cJSON* item;
    cJSON* tmpJSON;

    char* tmpStr;

    if ((NULL == sourceJSONObj) || (NULL == targetJSONObj))
        return ret;

    item = sourceJSONObj->child;
    while (item)
    {
        if (!item->string || (strlen(item->string) <= 0))
            continue;

        switch (item->type)
        {
        case cJSON_False:
            if (doesAttributeExist(targetJSONObj, item->string))
                cJSON_ReplaceItemInObject(targetJSONObj, item->string, cJSON_CreateFalse());
            else
                cJSON_AddItemToObject(targetJSONObj, item->string, cJSON_CreateFalse());
            break;
        case cJSON_True:
            if (doesAttributeExist(targetJSONObj, item->string))
                cJSON_ReplaceItemInObject(targetJSONObj, item->string, cJSON_CreateTrue());
            else
                cJSON_AddItemToObject(targetJSONObj, item->string, cJSON_CreateTrue());
            break;
        case cJSON_NULL:
            if (doesAttributeExist(targetJSONObj, item->string))
                cJSON_ReplaceItemInObject(targetJSONObj, item->string, cJSON_CreateNull());
            else
                cJSON_AddItemToObject(targetJSONObj, item->string, cJSON_CreateNull());
            break;
        case cJSON_Number:
            if (doesAttributeExist(targetJSONObj, item->string))
                cJSON_ReplaceItemInObject(targetJSONObj, item->string, cJSON_CreateNumber(
                item->valuedouble));
            else
                cJSON_AddItemToObject(targetJSONObj, item->string, cJSON_CreateNumber(
                item->valuedouble));
            break;
        case cJSON_String:
            if (doesAttributeExist(targetJSONObj, item->string))
                cJSON_ReplaceItemInObject(targetJSONObj, item->string, cJSON_CreateString(
                item->valuestring));
            else
                cJSON_AddItemToObject(targetJSONObj, item->string, cJSON_CreateString(
                item->valuestring));
            break;
        case cJSON_Array:
        case cJSON_Object:
            tmpStr = cJSON_Print(item);
            if (tmpStr && (strlen(tmpStr) > 0))
            {
                tmpJSON = cJSON_Parse((const char *) tmpStr);
                free(tmpStr);
                if (doesAttributeExist(targetJSONObj, item->string))
                    cJSON_ReplaceItemInObject(targetJSONObj, item->string, tmpJSON);
                else
                    cJSON_AddItemToObject(targetJSONObj, item->string, tmpJSON);
            }

            break;

        default:
            return false;
        }

        item = item->next;
    }

    ret = true;
    return ret;
}


#include <cmath>
#include <limits>

bool AreSame(double a, double b) {
    return std::fabs(a - b) < std::numeric_limits<double>::epsilon();
}

void diffJSONObject(cJSON *item1, cJSON *item2, const char* itemstring, cJSON *JSONObjDiff)
{
    if ((NULL == item1) || (NULL == item2))
        return;

    switch (item1->type)
    {
        case cJSON_False:
            if (item2)
            {
                bool valuebool = (item2->type == cJSON_True) ? true : false;
                if (valuebool)
                {
                    cJSON_AddItemToObject(JSONObjDiff, itemstring, cJSON_CreateString("False -> True"));
                }
            }
            else 
            {
                cJSON_AddItemToObject(JSONObjDiff, itemstring, cJSON_CreateString("n/a -> True"));
            }
            break;
        case cJSON_True:
            if (item2)
            {
                bool valuebool = (item2->type == cJSON_True) ? true : false;
                if (!valuebool)
                {
                    cJSON_AddItemToObject(JSONObjDiff, itemstring, cJSON_CreateString("True -> False"));
                }
            }
            else 
            {
                cJSON_AddItemToObject(JSONObjDiff, itemstring, cJSON_CreateString("n/a -> False"));
            }
            break;
        case cJSON_NULL:
            if (item2)
            {
                if (item2->type != cJSON_NULL)
                {
                    std::stringstream ss;
                    ss << "NULL -> ";
                    ss << item2->valuedouble;
                    cJSON_AddItemToObject(JSONObjDiff, itemstring, cJSON_CreateString(ss.str().c_str()));
                }
            }
            else
            {
                cJSON_AddItemToObject(JSONObjDiff, itemstring, cJSON_CreateString("n/a -> NULL"));
            }
            break;
        case cJSON_Number:
            if (item2)
            {
                if (!AreSame(item1->valuedouble, item2->valuedouble))
                {
                    std::stringstream ss;
                    ss << item1->valuedouble;
                    ss << " -> ";
                    ss << item2->valuedouble;
                    cJSON_AddItemToObject(JSONObjDiff, itemstring, cJSON_CreateString(ss.str().c_str()));
                }
            }
            else
            {
                std::stringstream ss;
                ss << "n/a -> ";
                ss << item2->valuedouble;
                cJSON_AddItemToObject(JSONObjDiff, itemstring, cJSON_CreateString(ss.str().c_str()));
            }
            break;
        case cJSON_String:
            if (item2)
            {
                std::string valuestring = item2->valuestring;
                if (valuestring.compare(item1->valuestring) != 0)
                {
                    std::stringstream ss;
                    ss << item1->valuestring;
                    ss << " -> ";
                    ss << valuestring;
                    cJSON_AddItemToObject(JSONObjDiff, itemstring, cJSON_CreateString(ss.str().c_str()));
                }
            }
            else
            {
                std::stringstream ss;
                ss << "n/a -> ";
                ss << item2->valuestring;
                cJSON_AddItemToObject(JSONObjDiff, itemstring, cJSON_CreateString(ss.str().c_str()));
            }
            break;
    }
}

bool diffJSONObjects(cJSON *JSONObj1, cJSON *JSONObj2, cJSON *JSONObjDiff)
{
    bool ret(false);
    cJSON* item1;

    if ((NULL == JSONObj2) || (NULL == JSONObj1))
        return ret;

    item1= JSONObj1->child;
    while (item1)
    {
        if (!item1->string || (strlen(item1->string) <= 0))
            continue;

        switch (item1->type)
        {
            case cJSON_Array:
                if (doesAttributeExist(JSONObj2, item1->string))
                {
                    cJSON* item2 = cJSON_GetObjectItem(JSONObj2, item1->string);
                    if (item2->type == cJSON_Array)
                    {
                        for (int idx = 0; idx < cJSON_GetArraySize(item1); idx++)
                        {
                            cJSON* item1_arrayItem = cJSON_GetArrayItem(item1, idx);
                            cJSON* item2_arrayItem = cJSON_GetArrayItem(item2, idx);
                            std::stringstream ss;
                            ss << item1->string << "[" << idx << "]";
                            diffJSONObject(item1_arrayItem, item2_arrayItem, ss.str().c_str(), JSONObjDiff);
                        }
                    }
                    else
                    {
                        std::stringstream ss;
                        ss << item2->string;
                        ss << ": Type mismatch (2nd item not an array)";
                        cJSON_AddItemToObject(JSONObjDiff, item2->string, cJSON_CreateString(ss.str().c_str()));
                    }
                }
                break;

            case cJSON_Object:
                if (doesAttributeExist(JSONObj2, item1->string))
                {
                    cJSON* JSONObj1_sub = cJSON_GetObjectItem(JSONObj1, item1->string);
                    cJSON* JSONObj2_sub = cJSON_GetObjectItem(JSONObj2, item1->string);
                    diffJSONObjects(JSONObj1_sub, JSONObj2_sub, JSONObjDiff);
                }
                else
                {
                    std::stringstream ss;
                    ss << item1->valuestring;
                    ss << " was added";
                    cJSON_AddItemToObject(JSONObjDiff, item1->string, cJSON_CreateString(ss.str().c_str()));
                }
                break;

            default:
                if (doesAttributeExist(JSONObj2, item1->string))
                {
                    cJSON* item2 = cJSON_GetObjectItem(JSONObj2, item1->string);
                    diffJSONObject(item1, item2, (const char*)item1->string, JSONObjDiff);
                }
                else 
                {
                    std::stringstream ss;
                    ss << item1->valuestring;
                    ss << " was added";
                    cJSON_AddItemToObject(JSONObjDiff, item1->string, cJSON_CreateString(ss.str().c_str()));
                }
                break;
        }

        item1 = item1->next;
    }

    ret = true;
    return ret;
}


bool copyItem(cJSON *targetJSONObj, char *targetKey, cJSON *sourceJSONObj,
              char *sourceKey)
{
    cJSON* item;
    cJSON* tmpJSON;
    char* tmpStr;

    item = cJSON_GetObjectItem(sourceJSONObj, sourceKey);
    if (!item)
        return false;

    switch (item->type)
    {
    case cJSON_False:
        if (doesAttributeExist(targetJSONObj, targetKey))
            cJSON_ReplaceItemInObject(targetJSONObj, targetKey, cJSON_CreateFalse());
        else
            cJSON_AddItemToObject(targetJSONObj, targetKey, cJSON_CreateFalse());
        break;
    case cJSON_True:
        if (doesAttributeExist(targetJSONObj, targetKey))
            cJSON_ReplaceItemInObject(targetJSONObj, targetKey, cJSON_CreateTrue());
        else
            cJSON_AddItemToObject(targetJSONObj, targetKey, cJSON_CreateTrue());
        break;
    case cJSON_NULL:
        if (doesAttributeExist(targetJSONObj, targetKey))
            cJSON_ReplaceItemInObject(targetJSONObj, targetKey, cJSON_CreateNull());
        else
            cJSON_AddItemToObject(targetJSONObj, targetKey, cJSON_CreateNull());
        break;
    case cJSON_Number:
        if (doesAttributeExist(targetJSONObj, targetKey))
            cJSON_ReplaceItemInObject(targetJSONObj, targetKey, cJSON_CreateNumber(
            item->valuedouble));
        else
            cJSON_AddItemToObject(targetJSONObj, targetKey, cJSON_CreateNumber(
            item->valuedouble));
        break;
    case cJSON_String:
        if (doesAttributeExist(targetJSONObj, targetKey))
            cJSON_ReplaceItemInObject(targetJSONObj, targetKey, cJSON_CreateString(
            item->valuestring));
        else
            cJSON_AddItemToObject(targetJSONObj, targetKey, cJSON_CreateString(
            item->valuestring));
        break;
    case cJSON_Array:
    case cJSON_Object:
        tmpStr = cJSON_Print(item);
        if (tmpStr && (strlen(tmpStr) > 0))
        {
            tmpJSON = cJSON_Parse((const char *) tmpStr);
            free(tmpStr);
            if (doesAttributeExist(targetJSONObj, targetKey))
                cJSON_ReplaceItemInObject(targetJSONObj, targetKey, tmpJSON);
            else
                cJSON_AddItemToObject(targetJSONObj, targetKey, tmpJSON);
        }

        break;

    default:
        return false;
    }

    return true;

}

void printJSON(cJSON *object)
{
    char* tmpStr;

    if (object)
    {
        tmpStr = cJSON_Print(object);
        printf("%s\n", tmpStr);
        fflush( stdout);
        free(tmpStr);
    }
}

void printJSONUnformatted(cJSON *object)
{
    char* tmpStr;

    if (object)
    {
        tmpStr = cJSON_PrintUnformatted(object);
        printf("%s\n", tmpStr);
        fflush( stdout);
        free(tmpStr);
    }
}

string JSON2Str(cJSON *object)
{
    stringstream ss;
    char* tmpStr = cJSON_PrintUnformatted(object);
    ss << tmpStr;
    free(tmpStr);
    return ss.str();
}


cJSONException::cJSONException(cJSON* obj, string msg)
{
    m_obj = obj;
    m_msg = msg;
}

const char* cJSONException::what() const throw()
{
    return (m_msg + "\nJSON object: " + string(cJSON_Print(m_obj))).c_str();
}

cJSON* getKey(cJSON* obj, string key)
{
    if (obj == NULL)
        return NULL;
    if (obj->type != cJSON_Object)
        return NULL;

    cJSON* c = obj->child;
    while (c && !(c->string == key))
        c = c->next;

    if (c == NULL) {
        throw cJSONException(obj, "key "+key+" not found");
    }

    return c;
}

cJSON* getItem(cJSON* obj, string key, int index)
{
    cJSON* array = getKey(obj, key);
    cJSON* result = cJSON_GetArrayItem(array, index);
    if (!result)
        throw cJSONException(obj, "wrong index");
    return result;
}

string asString(cJSON* obj)
{
    if (obj->type != cJSON_String)
        throw cJSONException(obj, "not a string");
    return obj->valuestring;
}

double asDouble(cJSON* obj)
{
    if (obj->type != cJSON_Number)
        throw cJSONException(obj, "not a number");
    return obj->valuedouble;
}

int asInt(cJSON* obj)
{
    if (obj->type != cJSON_Number)
        throw cJSONException(obj, "not a number");
    return obj->valueint;
}
