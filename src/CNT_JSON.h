#ifndef CNT_JSON__h
#define CNT_JSON__h

#include "cJSON.h"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <exception>
using namespace std;

#define INVALID_ID -9999

typedef cJSON* RPCObj;
typedef cJSON* RPC_RespObj;
typedef cJSON* RPC_Params;
typedef cJSON* RPC_Result;

typedef map<string, string> ARG;

// Synonyms for using cJSON objects as output or input/output when
// wrapped in Swig.
// - cJSON*: is only input, 
// - cJSON_out*: doesn't consume a parameter and it's returned in the output
// - cJSON_inout*: consumes the parameter, and returns the modification on the output
typedef cJSON cJSON_out;
typedef cJSON cJSON_inout;      /* Not implemented yet (maybe unnecessary) */

enum ResponseType_t
{
   RES_COMPLETE = 0, RES_UPDATE, RES_ERROR
};

RPCObj createRPCObj(string method, int id);
RPCObj createRPCObj(string method, int id, map<string, string> Args);

RPC_RespObj createRPC_RespObj(RPCObj RPC_Obj);
RPC_RespObj createRPC_RespObj(int id);

bool addParamsMap(RPCObj RPC_Obj, map<string, string> Args);
bool addParamsMap(RPCObj RPC_Obj, map<string, double> Args);

bool doesAttributeExist(cJSON* JSONObj, string AttributeKey, bool recurse = false);

int getAttributeDefault_Int(cJSON* JSONObj, string AttributeKey, int missing);
bool getAttributeValue_Int(cJSON* JSONObj, string AttributeKey, int &valueint);
bool getAttributeValue_IntArray(cJSON* JSONObj, string AttributeKey, int* valueint, unsigned int nValues);
bool setAttributeValue_Int(cJSON* JSONObj, string AttributeKey, int valueint);
bool incrAttributeValue_Int(cJSON* JSONObj, string AttributeKey, int incr = 1);

bool getAttributeValue_Bool(cJSON* JSONObj, string AttributeKey, bool &valuebool);
bool setAttributeValue_Bool(cJSON* JSONObj, string AttributeKey, bool valuebool);

double getAttributeDefault_Double(cJSON* JSONObj, string AttributeKey, double missing);
bool getAttributeValue_Double(cJSON* JSONObj, string AttributeKey, double &valuedouble);
bool setAttributeValue_Double(cJSON* JSONObj, string AttributeKey, double valuedouble);
bool incrAttributeValue_Double(cJSON* JSONObj, string AttributeKey, double incr = 1.0);

bool getAttributeValue_String(cJSON* JSONObj, string AttributeKey, string &stringval);
bool setAttributeValue_String(cJSON* JSONObj, string AttributeKey, string stringval);

bool MapToJSON_Str_Double(map<string, double> map_double, cJSON **JSONObj);

bool MapToJSON_Int_Double(map<int, double> map_double, cJSON **JSONObj);

bool JSONToMap_Str_Double(cJSON *JSONObj, map<string, double> *map_double);

bool JSONToMap_Int_Double(cJSON *JSONObj, map<int, double> *map_double);

bool writeJSONToFile(cJSON *object, string filename);
bool readJSONFromFile(cJSON **object, string filename);

bool mergeJSONObjects(cJSON *targetJSONObj, cJSON *sourceJSONObj);
bool diffJSONObjects(cJSON *JSONObj1, cJSON *JSONObj2, cJSON *JSONObjDiff);

bool copyItem(cJSON *targetJSONObj, char *targetKey, cJSON *sourceJSONObj,
      char *sourceKey);

void printJSON(cJSON *object);
void printJSONUnformatted(cJSON *object);

string JSON2Str(cJSON *object);

class cJSONException: public std::exception
{
 public:
   cJSONException(cJSON* obj, string msg);
   ~cJSONException() throw () { };
   virtual const char* what() const throw();
 private:
   cJSON* m_obj;
   string m_msg;
};

cJSON* getKey(cJSON* obj, string key);
cJSON* getItem(cJSON* obj, string key, int index);
string asString(cJSON* obj);
double asDouble(cJSON* obj);
int asInt(cJSON* obj);

#endif
