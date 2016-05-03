#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "json_c.h"


int main()
{
	const char* sample =
		"{"
		"	\"my_array\": ["
		"		\"Hello\","
		"			[\"World\"],"
		"			{ \"exclam\": \"!\" }"
		"	]"
		"}";
	
	struct json_error error;
	struct json_value* my_array = NULL;
	struct json_value* exclam = NULL;
	struct json_value root = json_parse(sample, strlen(sample), &error);

	// This is optional.
	json_malloc = &malloc;
	json_free = &free;

	if (error.failed_parse)
		printf("JSON Parse Error: %s @ line %d\n", error.message, error.line);
	else
	{
		my_array = json_get(&root.object_value, "my_array");
		exclam = json_get(&my_array->array_value.elements[2].object_value, "exclam");
		printf("%s %s%s\n", 
			   my_array->array_value.elements[0].string_value.value,
			   my_array->array_value.elements[1].array_value.elements[0].string_value.value,
			   exclam->string_value.value);
	}
	json_destroy(root);
	return 0;
}