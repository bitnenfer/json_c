#ifndef JSON_C_H
#define JSON_C_H 1

#ifndef JSON_C_BEGIN_EXTERN_C 
#define JSON_C_BEGIN_EXTERN_C extern "C" {
#endif
#ifndef JSON_C_END_EXTERN_C
#define JSON_C_END_EXTERN_C }
#endif

#if __cplusplus
JSON_C_BEGIN_EXTERN_C
#endif

#include <stddef.h>
/*
//////////////////////////////////////
//
// The user can define custom
// allocation routines. If none are
// defined then it'll fallback to
// C's standard library malloc & free
//
//////////////////////////////////////
*/
static void* (*json_malloc)(size_t) = NULL;
static void (*json_free)(void* ptr) = NULL;
/*
//////////////////////////////////////
//
// JSON Parse Structs.
//
//////////////////////////////////////
*/
struct json_pair;
struct json_value;

enum JSON_VALTYPE
{
	VAL_STRING,
	VAL_NUMBER,
	VAL_OBJECT,
	VAL_ARRAY,
	VAL_BOOL,
	VAL_NULL,
	VAL_VALUE
};
enum JSON_NUMTYPE
{
	NUM_FLOAT,
	NUM_INT
};
struct json_string
{
	char* value;
	size_t size;
};
struct json_number
{
	enum JSON_NUMTYPE type;
	union
	{
		double float_value;
		int int_value;
	};
};
struct json_bool
{
	unsigned char value : 1;
};
struct json_array
{
	struct json_value* elements;
	size_t array_size;
};
struct json_object
{
	struct json_pair* members;
	size_t object_count;
};
struct json_value
{
	enum JSON_VALTYPE type;
	union
	{
		struct json_number number_value;
		struct json_bool bool_value;
		struct json_string string_value;
		struct json_object object_value;
		struct json_array array_value;
	};
};
struct json_pair
{
	int key;
	struct json_value value;
};
struct json_error
{
	unsigned char failed_parse : 1;
	const char* message;
	int line;
};
/*
//////////////////////////////////////
//
// Public Declarations
//
//////////////////////////////////////
*/
struct json_value json_parse(const char* text, size_t text_size, struct json_error* error);
struct json_value* json_get(struct json_object* object, const char* key);
void json_destroy(struct json_value object);
int json_contains(struct json_object* object, const char* key);

#endif /*JSON_C_H*/
#if __cplusplus
JSON_C_END_EXTERN_C
#endif
