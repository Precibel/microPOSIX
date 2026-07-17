#ifndef MICROPOSIX_JSON_H
#define MICROPOSIX_JSON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "microposix/mm/memory_view.h"

/**
 * @file json.h
 * @brief JSON serialization and deserialization
 * 
 * Lightweight JSON parser and generator for embedded systems.
 * Supports parsing and generating JSON with minimal memory overhead.
 */

// Forward declarations
typedef struct mp_json_value mp_json_value_t;
typedef struct mp_json_object mp_json_object_t;
typedef struct mp_json_array mp_json_array_t;
typedef struct mp_json_parser mp_json_parser_t;
typedef struct mp_json_generator mp_json_generator_t;

/**
 * @brief JSON value types
 */
typedef enum {
    MP_JSON_NULL,
    MP_JSON_BOOLEAN,
    MP_JSON_NUMBER,
    MP_JSON_STRING,
    MP_JSON_OBJECT,
    MP_JSON_ARRAY,
    MP_JSON_ERROR
} mp_json_type_t;

/**
 * @brief JSON number value
 */
typedef struct {
    double value;           ///< Numeric value
    bool is_integer;       ///< Whether the value is an integer
    int64_t integer_value; ///< Integer value (if is_integer is true)
} mp_json_number_t;

/**
 * @brief JSON string value
 */
typedef struct {
    char *value;            ///< String value (null-terminated)
    size_t length;          ///< Length of string (excluding null terminator)
    bool owns_memory;       ///< Whether this struct owns the string memory
} mp_json_string_t;

/**
 * @brief JSON object (key-value pairs)
 */
struct mp_json_object {
    mp_json_value_t **keys;    ///< Array of key strings
    mp_json_value_t **values;  ///< Array of values
    size_t count;              ///< Number of key-value pairs
    size_t capacity;           ///< Allocated capacity
};

/**
 * @brief JSON array
 */
struct mp_json_array {
    mp_json_value_t **elements; ///< Array of elements
    size_t count;               ///< Number of elements
    size_t capacity;            ///< Allocated capacity
};

/**
 * @brief JSON value
 */
struct mp_json_value {
    mp_json_type_t type;       ///< Type of the value
    union {
        bool boolean;           ///< Boolean value
        mp_json_number_t number; ///< Number value
        mp_json_string_t string; ///< String value
        mp_json_object_t object; ///< Object value
        mp_json_array_t array;   ///< Array value
    } data;
};

/**
 * @brief JSON parser structure
 */
struct mp_json_parser {
    const char *input;          ///< Input JSON string
    size_t position;           ///< Current position in input
    size_t length;            ///< Total length of input
    mp_json_value_t *root;    ///< Root value (if parsing successful)
    const char *error;         ///< Error message (if parsing failed)
    size_t error_position;    ///< Position of error in input
    
    // Memory management
    void *(*alloc_fn)(size_t); ///< Allocation function
    void (*free_fn)(void *);   ///< Free function
    void *alloc_data;          ///< User data for alloc/free
};

/**
 * @brief JSON generator structure
 */
struct mp_json_generator {
    char *buffer;              ///< Output buffer
    size_t position;          ///< Current position in buffer
    size_t capacity;          ///< Total capacity of buffer
    bool pretty_print;        ///< Whether to use pretty printing
    int indent_level;         ///< Current indentation level
    
    // Memory management
    void *(*realloc_fn)(void *, size_t); ///< Reallocation function
    void *alloc_data;         ///< User data for realloc
};

/**
 * @brief JSON parsing flags
 */
typedef enum {
    MP_JSON_PARSE_STRICT = 0,      ///< Strict JSON parsing
    MP_JSON_PARSE_ALLOW_COMMENTS = (1 << 0),  ///< Allow C-style comments
    MP_JSON_PARSE_ALLOW_TRAILING_COMMA = (1 << 1),  ///< Allow trailing commas
    MP_JSON_PARSE_ALLOW_SINGLE_QUOTES = (1 << 2),  ///< Allow single quotes for strings
    MP_JSON_PARSE_ALLOW_UNQUOTED_KEYS = (1 << 3),  ///< Allow unquoted object keys
} mp_json_parse_flags_t;

/**
 * @brief JSON generation flags
 */
typedef enum {
    MP_JSON_GEN_COMPACT = 0,      ///< Compact output (no whitespace)
    MP_JSON_GEN_PRETTY = (1 << 0),  ///< Pretty print with indentation
    MP_JSON_GEN_SORT_KEYS = (1 << 1),  ///< Sort object keys alphabetically
    MP_JSON_GEN_ESCAPE_ALL = (1 << 2),  ///< Escape all special characters
} mp_json_gen_flags_t;

/**
 * @brief Initialize a JSON parser
 * 
 * @param parser The parser to initialize
 * @param input The JSON string to parse
 * @return 0 on success, -1 on error
 */
int mp_json_parser_init(mp_json_parser_t *parser, const char *input);

/**
 * @brief Initialize a JSON parser with custom allocator
 * 
 * @param parser The parser to initialize
 * @param input The JSON string to parse
 * @param alloc_fn Allocation function
 * @param free_fn Free function
 * @param alloc_data User data for allocator
 * @return 0 on success, -1 on error
 */
int mp_json_parser_init_with_allocator(mp_json_parser_t *parser, const char *input,
                                        void *(*alloc_fn)(size_t), void (*free_fn)(void *),
                                        void *alloc_data);

/**
 * @brief Parse JSON from a memory view
 * 
 * @param parser The parser to initialize
 * @param view The memory view containing JSON
 * @return 0 on success, -1 on error
 */
int mp_json_parser_init_from_view(mp_json_parser_t *parser, mp_memory_view_t *view);

/**
 * @brief Parse JSON
 * 
 * @param parser The parser
 * @param flags Parsing flags
 * @return The root JSON value, or NULL on error
 */
mp_json_value_t *mp_json_parse(mp_json_parser_t *parser, mp_json_parse_flags_t flags);

/**
 * @brief Free a parsed JSON value
 * 
 * @param value The value to free
 */
void mp_json_free(mp_json_value_t *value);

/**
 * @brief Destroy a JSON parser
 * 
 * @param parser The parser to destroy
 */
void mp_json_parser_destroy(mp_json_parser_t *parser);

/**
 * @brief Get the root value from a parser
 * 
 * @param parser The parser
 * @return The root value, or NULL if parsing failed
 */
mp_json_value_t *mp_json_parser_get_root(mp_json_parser_t *parser);

/**
 * @brief Get error information from a parser
 * 
 * @param parser The parser
 * @return Error message, or NULL if no error
 */
const char *mp_json_parser_get_error(mp_json_parser_t *parser);

/**
 * @brief Get error position from a parser
 * 
 * @param parser The parser
 * @return Position of error in input, or 0 if no error
 */
size_t mp_json_parser_get_error_position(mp_json_parser_t *parser);

/**
 * @brief Initialize a JSON generator
 * 
 * @param generator The generator to initialize
 * @param buffer Initial buffer (can be NULL)
 * @param capacity Initial buffer capacity
 * @return 0 on success, -1 on error
 */
int mp_json_generator_init(mp_json_generator_t *generator, char *buffer, size_t capacity);

/**
 * @brief Initialize a JSON generator with custom allocator
 * 
 * @param generator The generator to initialize
 * @param capacity Initial buffer capacity
 * @param realloc_fn Reallocation function
 * @param alloc_data User data for reallocator
 * @return 0 on success, -1 on error
 */
int mp_json_generator_init_with_allocator(mp_json_generator_t *generator, size_t capacity,
                                           void *(*realloc_fn)(void *, size_t), void *alloc_data);

/**
 * @brief Destroy a JSON generator
 * 
 * @param generator The generator to destroy
 */
void mp_json_generator_destroy(mp_json_generator_t *generator);

/**
 * @brief Set generation flags
 * 
 * @param generator The generator
 * @param flags Generation flags
 */
void mp_json_generator_set_flags(mp_json_generator_t *generator, mp_json_gen_flags_t flags);

/**
 * @brief Set indentation for pretty printing
 * 
 * @param generator The generator
 * @param indent Number of spaces for indentation
 */
void mp_json_generator_set_indent(mp_json_generator_t *generator, int indent);

/**
 * @brief Generate JSON from a value
 * 
 * @param generator The generator
 * @param value The value to generate
 * @return 0 on success, -1 on error
 */
int mp_json_generate(mp_json_generator_t *generator, mp_json_value_t *value);

/**
 * @brief Get the generated JSON string
 * 
 * @param generator The generator
 * @return The generated JSON string
 */
const char *mp_json_generator_get_output(mp_json_generator_t *generator);

/**
 * @brief Get the length of the generated JSON
 * 
 * @param generator The generator
 * @return Length of generated JSON
 */
size_t mp_json_generator_get_length(mp_json_generator_t *generator);

/**
 * @brief Reset a JSON generator
 * 
 * @param generator The generator to reset
 */
void mp_json_generator_reset(mp_json_generator_t *generator);

/**
 * @brief Create a new JSON null value
 * 
 * @return New JSON null value
 */
mp_json_value_t *mp_json_null(void);

/**
 * @brief Create a new JSON boolean value
 * 
 * @param value Boolean value
 * @return New JSON boolean value
 */
mp_json_value_t *mp_json_boolean(bool value);

/**
 * @brief Create a new JSON number value
 * 
 * @param value Numeric value
 * @return New JSON number value
 */
mp_json_value_t *mp_json_number(double value);

/**
 * @brief Create a new JSON integer value
 * 
 * @param value Integer value
 * @return New JSON integer value
 */
mp_json_value_t *mp_json_integer(int64_t value);

/**
 * @brief Create a new JSON string value (copies the string)
 * 
 * @param value String value
 * @return New JSON string value
 */
mp_json_value_t *mp_json_string(const char *value);

/**
 * @brief Create a new JSON string value with length (copies the string)
 * 
 * @param value String value
 * @param length Length of string
 * @return New JSON string value
 */
mp_json_value_t *mp_json_string_with_length(const char *value, size_t length);

/**
 * @brief Create a new JSON string value that takes ownership of the memory
 * 
 * @param value String value (will be freed when JSON value is freed)
 * @param length Length of string
 * @return New JSON string value
 */
mp_json_value_t *mp_json_string_owning(char *value, size_t length);

/**
 * @brief Create a new empty JSON object
 * 
 * @return New JSON object
 */
mp_json_value_t *mp_json_object(void);

/**
 * @brief Create a new empty JSON array
 * 
 * @return New JSON array
 */
mp_json_value_t *mp_json_array(void);

/**
 * @brief Add a key-value pair to a JSON object
 * 
 * @param object The JSON object
 * @param key The key string
 * @param value The value to add
 * @return 0 on success, -1 on error
 */
int mp_json_object_add(mp_json_value_t *object, const char *key, mp_json_value_t *value);

/**
 * @brief Add a key-value pair to a JSON object (takes ownership of key and value)
 * 
 * @param object The JSON object
 * @param key The key string (will be copied)
 * @param value The value to add (will be owned by object)
 * @return 0 on success, -1 on error
 */
int mp_json_object_add_owning(mp_json_value_t *object, const char *key, mp_json_value_t *value);

/**
 * @brief Get a value from a JSON object by key
 * 
 * @param object The JSON object
 * @param key The key to look up
 * @return The value, or NULL if not found
 */
mp_json_value_t *mp_json_object_get(mp_json_value_t *object, const char *key);

/**
 * @brief Check if a JSON object has a key
 * 
 * @param object The JSON object
 * @param key The key to check
 * @return true if the key exists
 */
bool mp_json_object_has(mp_json_value_t *object, const char *key);

/**
 * @brief Remove a key-value pair from a JSON object
 * 
 * @param object The JSON object
 * @param key The key to remove
 * @return The removed value, or NULL if not found
 */
mp_json_value_t *mp_json_object_remove(mp_json_value_t *object, const char *key);

/**
 * @brief Get the number of keys in a JSON object
 * 
 * @param object The JSON object
 * @return Number of keys
 */
size_t mp_json_object_size(mp_json_value_t *object);

/**
 * @brief Get all keys from a JSON object
 * 
 * @param object The JSON object
 * @return Array of key strings (caller must free)
 */
const char **mp_json_object_keys(mp_json_value_t *object);

/**
 * @brief Add an element to a JSON array
 * 
 * @param array The JSON array
 * @param value The value to add
 * @return 0 on success, -1 on error
 */
int mp_json_array_add(mp_json_value_t *array, mp_json_value_t *value);

/**
 * @brief Get an element from a JSON array by index
 * 
 * @param array The JSON array
 * @param index The index
 * @return The element, or NULL if out of bounds
 */
mp_json_value_t *mp_json_array_get(mp_json_value_t *array, size_t index);

/**
 * @brief Get the number of elements in a JSON array
 * 
 * @param array The JSON array
 * @return Number of elements
 */
size_t mp_json_array_size(mp_json_value_t *array);

/**
 * @brief Get the type of a JSON value
 * 
 * @param value The JSON value
 * @return The type
 */
mp_json_type_t mp_json_get_type(mp_json_value_t *value);

/**
 * @brief Get boolean value from a JSON value
 * 
 * @param value The JSON value
 * @return Boolean value (only valid if type is MP_JSON_BOOLEAN)
 */
bool mp_json_get_boolean(mp_json_value_t *value);

/**
 * @brief Get number value from a JSON value
 * 
 * @param value The JSON value
 * @return Numeric value (only valid if type is MP_JSON_NUMBER)
 */
double mp_json_get_number(mp_json_value_t *value);

/**
 * @brief Get integer value from a JSON value
 * 
 * @param value The JSON value
 * @return Integer value (only valid if type is MP_JSON_NUMBER and is_integer is true)
 */
int64_t mp_json_get_integer(mp_json_value_t *value);

/**
 * @brief Get string value from a JSON value
 * 
 * @param value The JSON value
 * @return String value (only valid if type is MP_JSON_STRING)
 */
const char *mp_json_get_string(mp_json_value_t *value);

/**
 * @brief Get string length from a JSON value
 * 
 * @param value The JSON value
 * @return String length (only valid if type is MP_JSON_STRING)
 */
size_t mp_json_get_string_length(mp_json_value_t *value);

/**
 * @brief Check if a JSON value is null
 * 
 * @param value The JSON value
 * @return true if the value is null
 */
bool mp_json_is_null(mp_json_value_t *value);

/**
 * @brief Check if a JSON value is a boolean
 * 
 * @param value The JSON value
 * @return true if the value is a boolean
 */
bool mp_json_is_boolean(mp_json_value_t *value);

/**
 * @brief Check if a JSON value is a number
 * 
 * @param value The JSON value
 * @return true if the value is a number
 */
bool mp_json_is_number(mp_json_value_t *value);

/**
 * @brief Check if a JSON value is a string
 * 
 * @param value The JSON value
 * @return true if the value is a string
 */
bool mp_json_is_string(mp_json_value_t *value);

/**
 * @brief Check if a JSON value is an object
 * 
 * @param value The JSON value
 * @return true if the value is an object
 */
bool mp_json_is_object(mp_json_value_t *value);

/**
 * @brief Check if a JSON value is an array
 * 
 * @param value The JSON value
 * @return true if the value is an array
 */
bool mp_json_is_array(mp_json_value_t *value);

/**
 * @brief Deep copy a JSON value
 * 
 * @param value The value to copy
 * @return New copy of the value
 */
mp_json_value_t *mp_json_deep_copy(mp_json_value_t *value);

/**
 * @brief Compare two JSON values for equality
 * 
 * @param a First value
 * @param b Second value
 * @return true if values are equal
 */
bool mp_json_equal(mp_json_value_t *a, mp_json_value_t *b);

/**
 * @brief Convert a JSON value to a string
 * 
 * @param value The value to convert
 * @param flags Generation flags
 * @return Newly allocated string (caller must free)
 */
char *mp_json_to_string(mp_json_value_t *value, mp_json_gen_flags_t flags);

/**
 * @brief Parse JSON from a file
 * 
 * @param filename The file to parse
 * @param flags Parsing flags
 * @return Root JSON value, or NULL on error
 */
mp_json_value_t *mp_json_parse_file(const char *filename, mp_json_parse_flags_t flags);

/**
 * @brief Write JSON to a file
 * 
 * @param filename The file to write to
 * @param value The JSON value to write
 * @param flags Generation flags
 * @return 0 on success, -1 on error
 */
int mp_json_write_file(const char *filename, mp_json_value_t *value, mp_json_gen_flags_t flags);

/**
 * @brief Helper macro to check JSON type
 */
#define MP_JSON_TYPE_CHECK(value, type) (mp_json_get_type(value) == (type))

/**
 * @brief Helper macro to get JSON value as a specific type
 */
#define MP_JSON_AS(type, value) ((type *)(&(value)->data))

/**
 * @brief Helper macro to iterate over JSON object keys
 */
#define MP_JSON_OBJECT_FOREACH(object, key, value) \
    for (size_t _i = 0; _i < (object)->data.object.count && \
         ((key) = mp_json_get_string((object)->data.object.keys[_i]), \
          (value) = (object)->data.object.values[_i], 1); \
         _i++)

/**
 * @brief Helper macro to iterate over JSON array elements
 */
#define MP_JSON_ARRAY_FOREACH(array, value) \
    for (size_t _i = 0; _i < (array)->data.array.count && \
         ((value) = (array)->data.array.elements[_i], 1); \
         _i++)

#endif // MICROPOSIX_JSON_H
