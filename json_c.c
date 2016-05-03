#include "json_c.h"

#ifndef JSON_C_FORCEINLINE
#if _MSC_VER
#define JSON_C_FORCEINLINE __forceinline
#else
#define JSON_C_FORCEINLINE inline
#endif
#endif
#ifndef JSON_C_NEW
#define JSON_C_NEW(T) (T*)json_malloc(sizeof(T))
#define JSON_C_NEWARRAY(T, C) (T*)json_malloc(sizeof(T) * C)
#endif

//////////////////////////////////////
//
// JSON Lexer Structs
//
//////////////////////////////////////

enum JSON_TOKTYPE
{
	TOKTYPE_FLOAT,
	TOKTYPE_INT,
	TOKTYPE_STRING,
	TOKTYPE_TRUE,
	TOKTYPE_FALSE,
	TOKTYPE_NULL,
	TOKTYPE_LBRACE,
	TOKTYPE_RBRACE,
	TOKTYPE_LSQBR,
	TOKTYPE_RSQBR,
	TOKTYPE_COLON,
	TOKTYPE_COMMA
};

struct json_lex_token
{
	struct json_lex_token* next_token;

	enum JSON_TOKTYPE token_type;
	size_t chars_count;
	size_t line;
	union
	{
		double float_value;
		int int_value;
		char* chars_value;
		unsigned int bool_value : 1;
	};
};

struct json_lex_tokenstream
{
	struct json_lex_token* head;
	struct json_lex_token* next;
};

//////////////////////////////////////
//
// Private Declarations
//
//////////////////////////////////////

void json_util_append_token(struct json_lex_tokenstream* stream, struct json_lex_token* token);
struct json_lex_token* json_util_newtoken(enum JSON_TOKTYPE token_type);
struct json_lex_tokenstream* json_util_newtokenstream();
void json_util_free_token(struct json_lex_token* token);
void json_util_free_tokenstream(struct json_lex_tokenstream* stream);
struct json_array json_parse_array(struct json_lex_token** token, struct json_error* error);
struct json_value json_parse_value(struct json_lex_token** token, struct json_error* error);
void json_parse_members(struct json_lex_token** token, struct json_pair* store, size_t element_count, struct json_error* error);
struct json_object json_parse_object(struct json_lex_token** token, struct json_error* error);
void json_free_value(struct json_value value);

//////////////////////////////////////
//
// JSON Errors
//
//////////////////////////////////////

#define ERR_JSON_MSG_LEX_INVALID_TOKEN "Invalid Token"
#define ERR_JSON_MSG_PAR_INVALID_VALUE "Invalid Value"
#define ERR_JSON_MSG_PAR_MISSING_RSQBR "Missing ]"
#define ERR_JSON_MSG_PAR_MISSING_COMMA "Missing ,"
#define ERR_JSON_MSG_PAR_INVALID_PAIR "Invalid pair value"
#define ERR_JSON_MSG_PAR_MISSING_LBRACE "Missing }"

static size_t json_global_line = 1;

void json_emit_error(struct json_error* error, struct json_lex_token* token, const char* error_msg)
{
	if (error != NULL && !error->failed_parse)
	{
		error->failed_parse = 1;
		if (token != NULL)
			error->line = token->line;
		else
			error->line = json_global_line;
		error->message = error_msg;
	}
}

//////////////////////////////////////
//
// JSON Utility Functions
//
//////////////////////////////////////

JSON_C_FORCEINLINE void json_util_append_token(struct json_lex_tokenstream* stream, struct json_lex_token* token)
{
	token->line = json_global_line;
	if (stream->head == NULL)
	{
		stream->head = token;
		stream->next = token;
	}
	else
	{
		stream->next->next_token = token;
		stream->next = token;
	}
}

JSON_C_FORCEINLINE char* json_util_copystr(const char* str, size_t size)
{
	char* new_str = NULL;
	new_str = json_malloc(size + 1);
	memmove(new_str, str, size);
	new_str[size] = 0;
	return new_str;
}

JSON_C_FORCEINLINE struct json_lex_token* json_util_newtoken(enum JSON_TOKTYPE token_type)
{
	struct json_lex_token* token = NULL;
	token = (struct json_lex_token*)json_malloc(sizeof(struct json_lex_token));
	token->next_token = NULL;
	token->token_type = token_type;
	return token;
}

JSON_C_FORCEINLINE struct json_lex_tokenstream* json_util_newtokenstream()
{
	struct json_lex_tokenstream* stream = NULL;
	stream = (struct json_lex_tokenstream*)json_malloc(sizeof(struct json_lex_tokenstream));
	stream->head = NULL;
	stream->next = NULL;
	return stream;
}

JSON_C_FORCEINLINE void json_util_free_token(struct json_lex_token* token)
{
	if (token != NULL)
	{
		if (token->token_type == TOKTYPE_STRING)
			json_free(token->chars_value);
		json_util_free_token(token->next_token);
		json_free(token);
	}
}

JSON_C_FORCEINLINE void json_util_free_tokenstream(struct json_lex_tokenstream* stream)
{
	if (stream != NULL)
	{
		json_util_free_token(stream->head);
		json_free(stream);
	}
}

//////////////////////////////////////
//
// JSON Lexer Functions
//
//////////////////////////////////////

JSON_C_FORCEINLINE int json_find(const char* text, char test)
{
	int index = 0;
	char current = text[index];
	while (current != 0)
	{
		if ((current = text[index++]) == test)
		{
			return 1;
		}
	}

	return 0;
}

int json_scan_string(struct json_lex_token** token, const char* text, size_t* lex_index)
{
	size_t index = *lex_index;
	char current = text[index];
	if (current == '"')
	{
		size_t start = ++index;
		current = text[index];
		while (current != '"' && current != 0)
			current = text[++index];
		*token = json_util_newtoken(TOKTYPE_STRING);
		(*token)->chars_value = json_util_copystr(&text[start], index - start);
		(*token)->chars_count = index - start;
		*lex_index = index;
		return 1;
	}
	return 0;
}

int json_scan_int(struct json_lex_token** token, const char* text, size_t* lex_index)
{
	static const char* DIGITS = "-0123456789";
	size_t index = *lex_index;
	char current = text[index];
	if (json_find(DIGITS, current))
	{
		size_t start = index++;
		current = text[index];
		while (json_find(DIGITS, current) && current != 0)
			current = text[++index];
		*token = json_util_newtoken(TOKTYPE_INT);
		(*token)->int_value = atoi(&text[start]);
		*lex_index = index - 1;
		return 1;
	}
	return 0;
}

int json_scan_float(struct json_lex_token** token, const char* text, size_t* lex_index)
{
	static const char* DIGITS = ".-+0123456789eE";
	size_t index = *lex_index;
	char current = text[index];
	if (json_find(DIGITS, current))
	{
		size_t start = index++;
		current = text[index];
		int found_dot = 0;
		while (json_find(DIGITS, current) && current != 0)
		{
			if (!found_dot)
				found_dot = current == '.';
			current = text[++index];
		}
		if (!found_dot)
			return 0;
		*token = json_util_newtoken(TOKTYPE_FLOAT);
		(*token)->float_value = atof(&text[start]);
		*lex_index = index - 1;
		return 1;
	}
	return 0;
}

int json_scan_true(struct json_lex_token** token, const char* text, size_t* lex_index)
{
	static const char* TRUE_NAME = "true";
	size_t index = *lex_index;
	if (text[index] == TRUE_NAME[0])
	{
		int i;
		for (i = 1; i < 4; ++i)
		{
			if (text[index + i] != TRUE_NAME[i])
				return 0;
		}
		*token = json_util_newtoken(TOKTYPE_TRUE);
		(*token)->bool_value = 1;
		*lex_index += 3;
		return 1;
	}
	return 0;
}

int json_scan_false(struct json_lex_token** token, const char* text, size_t* lex_index)
{
	static const char* FALSE_NAME = "false";
	size_t index = *lex_index;
	if (text[index] == FALSE_NAME[0])
	{
		int i;
		for (i = 1; i < 5; ++i)
		{
			if (text[index + i] != FALSE_NAME[i])
				return 0;
		}
		*token = json_util_newtoken(TOKTYPE_FALSE);
		(*token)->bool_value = 0;
		*lex_index += 4;
		return 1;
	}
	return 0;
}

int json_scan_null(struct json_lex_token** token, const char* text, size_t* lex_index)
{
	static const char* NULL_NAME = "null";
	size_t index = *lex_index;
	if (text[index] == NULL_NAME[0])
	{
		int i;
		for (i = 1; i < 4; ++i)
		{
			if (text[index + i] != NULL_NAME[i])
				return 0;
		}
		*token = json_util_newtoken(TOKTYPE_NULL);
		*lex_index += 3;
		return 1;
	}
	return 0;
}

int json_scan_symbols(struct json_lex_token** token, const char* text, size_t* lex_index)
{
	switch (text[*lex_index])
	{
		case ',':
			*token = json_util_newtoken(TOKTYPE_COMMA);
			return 1;
		case '{':
			*token = json_util_newtoken(TOKTYPE_LBRACE);
			return 1;
		case '}':
			*token = json_util_newtoken(TOKTYPE_RBRACE);
			return 1;
		case '[':
			*token = json_util_newtoken(TOKTYPE_LSQBR);
			return 1;
		case ']':
			*token = json_util_newtoken(TOKTYPE_RSQBR);
			return 1;
		case ':':
			*token = json_util_newtoken(TOKTYPE_COLON);
			return 1;
	}
	return 0;
}

int json_scan_empty(const char* text, size_t* lex_index)
{
	static const char* EMPTY_NAME = " \n\t\r";
	size_t index = *lex_index;
	char current = text[index++];
	if (json_find(EMPTY_NAME, current) || current == 0)
	{
		if (current == '\n')
			++json_global_line;
		current = text[index++];
		while (json_find(EMPTY_NAME, current) || current == 0)
		{
			if (current == '\n')
				++json_global_line;
			current = text[index++];
		}
		*lex_index = index - 1;
		return 1;
	}
	return 0;
}

struct json_lex_tokenstream* json_lex(const char* text, size_t text_size, struct json_error* error)
{
	struct json_lex_tokenstream* stream = NULL;
	size_t index = 0;
	stream = json_util_newtokenstream();
	json_global_line = 1;
	while (index < text_size)
	{
		size_t last_index = index;
		struct json_lex_token* token = NULL;
		if (json_scan_empty(text, &index)) continue;
		else if (json_scan_string(&token, text, &index));
		else if (json_scan_null(&token, text, &index));
		else if (json_scan_false(&token, text, &index));
		else if (json_scan_true(&token, text, &index));
		else if (json_scan_float(&token, text, &index));
		else if (json_scan_int(&token, text, &index));
		else if (json_scan_symbols(&token, text, &index));
		else
		{
			json_emit_error(error, NULL, ERR_JSON_MSG_LEX_INVALID_TOKEN);
			json_util_free_tokenstream(stream);
			return NULL;
		}
		if (token != NULL)
			json_util_append_token(stream, token);

		++index;
	}
	return stream;
}

//////////////////////////////////////
//
// JSON Parser Functions
//
//////////////////////////////////////

JSON_C_FORCEINLINE int json_simple_hash(const char* string, size_t len)
{
	int hash = 5301;
	int i = len;
	while (i)
		hash = (hash * 33) ^ string[--i];

	return hash;
}

void json_free_value(struct json_value value)
{
	size_t index;
	if (value.type == VAL_STRING)
	{
		json_free(value.string_value.value);
	}
	else if (value.type == VAL_OBJECT)
	{
		for (index = 0; index < value.object_value.object_count; ++index)
		{
			json_free_value(value.object_value.members[index].value);
		}
		json_free(value.object_value.members);
	}
	else if (value.type == VAL_ARRAY)
	{
		for (index = 0; index < value.array_value.array_size; ++index)
		{
			json_free_value(value.array_value.elements[index]);
		}
		json_free(value.array_value.elements);
	}
}

struct json_value json_parse_value(struct json_lex_token** token, struct json_error* error)
{
	struct json_value value;
	if ((*token)->token_type == TOKTYPE_STRING)
	{
		value.type = VAL_STRING;
		value.string_value.value = json_util_copystr((*token)->chars_value, (*token)->chars_count);
		value.string_value.size = (*token)->chars_count;
		(*token) = (*token)->next_token;
	}
	else if ((*token)->token_type == TOKTYPE_FLOAT)
	{
		value.type = VAL_NUMBER;
		value.number_value.float_value = (*token)->float_value;
		value.number_value.type = NUM_FLOAT;
		(*token) = (*token)->next_token;
	}
	else if ((*token)->token_type == TOKTYPE_INT)
	{
		value.type = VAL_NUMBER;
		value.number_value.int_value = (*token)->int_value;
		value.number_value.type = NUM_INT;
		(*token) = (*token)->next_token;
	}
	else if ((*token)->token_type == TOKTYPE_FALSE)
	{
		value.type = VAL_BOOL;
		value.bool_value.value = (*token)->bool_value;
		(*token) = (*token)->next_token;
	}
	else if ((*token)->token_type == TOKTYPE_TRUE)
	{
		value.type = VAL_BOOL;
		value.bool_value.value = (*token)->bool_value;
		(*token) = (*token)->next_token;
	}
	else if ((*token)->token_type == TOKTYPE_NULL)
	{
		value.type = VAL_NULL;
		(*token) = (*token)->next_token;
	}
	else if ((*token)->token_type == TOKTYPE_LBRACE)
	{
		value.type = VAL_OBJECT;
		value.object_value = json_parse_object(token, error);
	}
	else if ((*token)->token_type == TOKTYPE_LSQBR)
	{
		value.type = VAL_ARRAY;
		value.array_value = json_parse_array(token, error);
	}
	else
	{
		json_emit_error(error, *token, ERR_JSON_MSG_PAR_INVALID_VALUE);
	}
	return value;
}

size_t json_list_size(struct json_lex_token* token)
{
	size_t elem_count = 0;
	struct json_lex_token* tok = token;
	do
	{
		tok = tok->next_token;
		if (tok->token_type == TOKTYPE_LSQBR)
		{
			tok = tok->next_token;
			int count = 1;
			while (tok != NULL && count > 0)
			{
				if (tok->token_type == TOKTYPE_RSQBR)
					--count;
				else if (tok->token_type == TOKTYPE_LSQBR)
					++count;
				tok = tok->next_token;
			}
		}
		else if (tok->token_type == TOKTYPE_LBRACE)
		{
			tok = tok->next_token;
			int count = 1;
			while (tok != NULL && count > 0)
			{
				if (tok->token_type == TOKTYPE_RBRACE)
					--count;
				else if (tok->token_type == TOKTYPE_LBRACE)
					++count;
				tok = tok->next_token;
			}
		}
		else if (tok->token_type != TOKTYPE_COMMA)
		{
			tok = tok->next_token;
		}
		++elem_count;
	} while (tok != NULL && tok->token_type == TOKTYPE_COMMA);
	return elem_count;
}

void json_parse_list(struct json_lex_token** token, struct json_value* store, size_t element_count, struct json_error* error)
{
	size_t index = 0;
	struct json_lex_token* last_token = NULL;
	if (element_count == 0)
		return;
	store[index++] = json_parse_value(token, error);
	last_token = (*token);
	while (index < element_count &&
		(*token)->token_type == TOKTYPE_COMMA)
	{
		*token = (*token)->next_token;
		store[index++] = json_parse_value(token, error);
		last_token = *token;
	}
	*token = last_token;
}

struct json_array json_parse_array(struct json_lex_token** token, struct json_error* error)
{
	struct json_array arr;
	struct json_lex_token* tok = *token;
	size_t elem_count = 0;
	if (tok->token_type == TOKTYPE_LSQBR)
	{
		elem_count = json_list_size(tok);
		tok = tok->next_token;
		arr.array_size = elem_count;
		arr.elements = JSON_C_NEWARRAY(struct json_value, elem_count);
		json_parse_list(&tok, arr.elements, elem_count, error);
		if (tok == NULL || tok->token_type != TOKTYPE_RSQBR)
		{
			json_emit_error(error, tok, ERR_JSON_MSG_PAR_MISSING_RSQBR);
			return arr;
		}
		*token = tok->next_token;
	}
	return arr;
}

struct json_pair json_parse_pair(struct json_lex_token** token, struct json_error* error)
{
	struct json_pair pair;
	if ((*token)->token_type == TOKTYPE_STRING &&
		(*token)->next_token != NULL &&
		(*token)->next_token->token_type == TOKTYPE_COLON)
	{
		pair.key = json_simple_hash((*token)->chars_value, (*token)->chars_count);
		(*token) = (*token)->next_token->next_token;
		pair.value = json_parse_value(token, error);
	}
	else
	{
		json_emit_error(error, *token, ERR_JSON_MSG_PAR_INVALID_PAIR);
	}
	return pair;
}

void json_parse_members(struct json_lex_token** token, struct json_pair* store, size_t element_count, struct json_error* error)
{
	size_t index = 0;
	struct json_lex_token* last_token = NULL;
	if (element_count == 0)
		return;
	store[index++] = json_parse_pair(token, error);
	last_token = (*token);
	while (index < element_count &&
		(*token)->token_type == TOKTYPE_COMMA)
	{
		*token = (*token)->next_token;
		store[index++] = json_parse_pair(token, error);
		last_token = *token;
	}
	*token = last_token;
}

struct json_object json_parse_object(struct json_lex_token** token, struct json_error* error)
{
	struct json_object obj;
	struct json_lex_token* tok = *token;
	size_t elem_count = 0;
	if (tok->token_type == TOKTYPE_LBRACE)
	{
		elem_count = json_list_size(tok);
		tok = tok->next_token;
		obj.object_count = elem_count;
		obj.members = JSON_C_NEWARRAY(struct json_pair, elem_count);
		json_parse_members(&tok, obj.members, elem_count, error);
		if (tok == NULL || tok->token_type != TOKTYPE_RBRACE)
		{
			json_emit_error(error, tok, ERR_JSON_MSG_PAR_MISSING_LBRACE);
			return obj;
		}
		*token = tok->next_token;
	}
	return obj;
}

struct json_value json_parse(const char* text, size_t text_size, struct json_error* error)
{
	struct json_value root = { VAL_NULL };
	struct json_lex_tokenstream* stream = NULL;
	size_t line = 0;
	if (error != NULL)
		error->failed_parse = 0;
	if (json_malloc == NULL)
		json_malloc = &malloc;
	if (json_free == NULL)
		json_free = &free;
	stream = json_lex(text, text_size, error);
	if (stream == NULL)
	{
		return root;
	}
	root = json_parse_value(&(stream->head), error);
	json_util_free_tokenstream(stream);
	if (error->failed_parse)
	{
		json_free_value(root);
		return root;
	}
	return root;
}
struct json_value* json_get(struct json_object* object, const char* key)
{
	size_t index;
	int hash = json_simple_hash(key, strlen(key));
	for (index = 0; index < object->object_count; ++index)
	{
		if (object->members[index].key == hash)
			return &object->members[index].value;
	}
	return NULL;
}
void json_destroy(struct json_value object)
{
	json_free_value(object);
}
int json_contains(struct json_object* object, const char* key)
{
	return json_get(object, key) != NULL;
}
