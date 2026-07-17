#include <stdio.h>
#include "microposix/serialization/json.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

// Helper macros
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Default allocator functions
static void *default_alloc(size_t size) {
    return malloc(size);
}

static void default_free(void *ptr) {
    free(ptr);
}

static void *default_realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}

// Character classification
static inline bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static inline bool is_hex_digit(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// Skip whitespace
static void skip_whitespace(mp_json_parser_t *parser) {
    while (parser->position < parser->length && 
           is_whitespace(parser->input[parser->position])) {
        parser->position++;
    }
}

// Skip comments (if allowed)
static bool skip_comments(mp_json_parser_t *parser, mp_json_parse_flags_t flags) {
    if (!(flags & MP_JSON_PARSE_ALLOW_COMMENTS)) {
        return false;
    }
    
    if (parser->position + 1 < parser->length) {
        if (parser->input[parser->position] == '/' && 
            parser->input[parser->position + 1] == '/') {
            // Single-line comment
            parser->position += 2;
            while (parser->position < parser->length && 
                   parser->input[parser->position] != '\n') {
                parser->position++;
            }
            return true;
        } else if (parser->input[parser->position] == '/' && 
                   parser->input[parser->position + 1] == '*') {
            // Multi-line comment
            parser->position += 2;
            while (parser->position + 1 < parser->length) {
                if (parser->input[parser->position] == '*' && 
                    parser->input[parser->position + 1] == '/') {
                    parser->position += 2;
                    return true;
                }
                parser->position++;
            }
            // Unterminated comment
            parser->error = "Unterminated comment";
            parser->error_position = parser->position;
            return false;
        }
    }
    
    return false;
}

// Parse a JSON value
static mp_json_value_t *parse_value(mp_json_parser_t *parser, mp_json_parse_flags_t flags);

// Parse a JSON object
static mp_json_value_t *parse_object(mp_json_parser_t *parser, mp_json_parse_flags_t flags) {
    mp_json_value_t *value = (mp_json_value_t *)parser->alloc_fn(sizeof(mp_json_value_t));
    if (!value) {
        parser->error = "Out of memory";
        parser->error_position = parser->position;
        return NULL;
    }
    
    value->type = MP_JSON_OBJECT;
    value->data.object.keys = NULL;
    value->data.object.values = NULL;
    value->data.object.count = 0;
    value->data.object.capacity = 0;
    
    // Expect '{'
    if (parser->position >= parser->length || parser->input[parser->position] != '{') {
        parser->error = "Expected '{'";
        parser->error_position = parser->position;
        parser->alloc_fn(sizeof(mp_json_value_t)); // Free value
        return NULL;
    }
    parser->position++;
    
    // Parse key-value pairs
    while (1) {
        skip_whitespace(parser);
        
        // Check for comments
        while (skip_comments(parser, flags)) {
            skip_whitespace(parser);
        }
        
        // Check for end of object
        if (parser->position >= parser->length) {
            parser->error = "Unterminated object";
            parser->error_position = parser->position;
            break;
        }
        
        if (parser->input[parser->position] == '}') {
            parser->position++;
            break;
        }
        
        // Parse key
        mp_json_value_t *key = parse_value(parser, flags);
        if (!key) {
            // Free partially parsed object
            for (size_t i = 0; i < value->data.object.count; i++) {
                parser->free_fn(value->data.object.keys[i]);
                parser->free_fn(value->data.object.values[i]);
            }
            parser->free_fn(value->data.object.keys);
            parser->free_fn(value->data.object.values);
            parser->free_fn(value);
            return NULL;
        }
        
        // Key must be a string
        if (key->type != MP_JSON_STRING) {
            parser->error = "Object key must be a string";
            parser->error_position = parser->position;
            parser->free_fn(key);
            for (size_t i = 0; i < value->data.object.count; i++) {
                parser->free_fn(value->data.object.keys[i]);
                parser->free_fn(value->data.object.values[i]);
            }
            parser->free_fn(value->data.object.keys);
            parser->free_fn(value->data.object.values);
            parser->free_fn(value);
            return NULL;
        }
        
        skip_whitespace(parser);
        
        // Check for colon
        if (parser->position >= parser->length || parser->input[parser->position] != ':') {
            parser->error = "Expected ':'";
            parser->error_position = parser->position;
            parser->free_fn(key);
            for (size_t i = 0; i < value->data.object.count; i++) {
                parser->free_fn(value->data.object.keys[i]);
                parser->free_fn(value->data.object.values[i]);
            }
            parser->free_fn(value->data.object.keys);
            parser->free_fn(value->data.object.values);
            parser->free_fn(value);
            return NULL;
        }
        parser->position++;
        
        // Parse value
        mp_json_value_t *val = parse_value(parser, flags);
        if (!val) {
            parser->free_fn(key);
            for (size_t i = 0; i < value->data.object.count; i++) {
                parser->free_fn(value->data.object.keys[i]);
                parser->free_fn(value->data.object.values[i]);
            }
            parser->free_fn(value->data.object.keys);
            parser->free_fn(value->data.object.values);
            parser->free_fn(value);
            return NULL;
        }
        
        // Add key-value pair to object
        if (value->data.object.count >= value->data.object.capacity) {
            size_t new_capacity = value->data.object.capacity == 0 ? 8 : value->data.object.capacity * 2;
            mp_json_value_t **new_keys = (mp_json_value_t **)parser->alloc_fn(
                new_capacity * sizeof(mp_json_value_t *));
            mp_json_value_t **new_values = (mp_json_value_t **)parser->alloc_fn(
                new_capacity * sizeof(mp_json_value_t *));
            
            if (!new_keys || !new_values) {
                parser->error = "Out of memory";
                parser->error_position = parser->position;
                parser->free_fn(key);
                parser->free_fn(val);
                for (size_t i = 0; i < value->data.object.count; i++) {
                    parser->free_fn(value->data.object.keys[i]);
                    parser->free_fn(value->data.object.values[i]);
                }
                if (new_keys) parser->free_fn(new_keys);
                if (new_values) parser->free_fn(new_values);
                parser->free_fn(value->data.object.keys);
                parser->free_fn(value->data.object.values);
                parser->free_fn(value);
                return NULL;
            }
            
            for (size_t i = 0; i < value->data.object.count; i++) {
                new_keys[i] = value->data.object.keys[i];
                new_values[i] = value->data.object.values[i];
            }
            
            parser->free_fn(value->data.object.keys);
            parser->free_fn(value->data.object.values);
            value->data.object.keys = new_keys;
            value->data.object.values = new_values;
            value->data.object.capacity = new_capacity;
        }
        
        value->data.object.keys[value->data.object.count] = key;
        value->data.object.values[value->data.object.count] = val;
        value->data.object.count++;
        
        skip_whitespace(parser);
        
        // Check for comma or end
        if (parser->position < parser->length && parser->input[parser->position] == ',') {
            parser->position++;
            continue;
        } else if (parser->position < parser->length && parser->input[parser->position] == '}') {
            continue;
        } else if (flags & MP_JSON_PARSE_ALLOW_TRAILING_COMMA) {
            // Allow trailing comma
            if (parser->position < parser->length && parser->input[parser->position] == '}') {
                continue;
            }
        }
    }
    
    return value;
}

// Parse a JSON array
static mp_json_value_t *parse_array(mp_json_parser_t *parser, mp_json_parse_flags_t flags) {
    mp_json_value_t *value = (mp_json_value_t *)parser->alloc_fn(sizeof(mp_json_value_t));
    if (!value) {
        parser->error = "Out of memory";
        parser->error_position = parser->position;
        return NULL;
    }
    
    value->type = MP_JSON_ARRAY;
    value->data.array.elements = NULL;
    value->data.array.count = 0;
    value->data.array.capacity = 0;
    
    // Expect '['
    if (parser->position >= parser->length || parser->input[parser->position] != '[') {
        parser->error = "Expected '['";
        parser->error_position = parser->position;
        parser->free_fn(value);
        return NULL;
    }
    parser->position++;
    
    // Parse elements
    while (1) {
        skip_whitespace(parser);
        
        // Check for comments
        while (skip_comments(parser, flags)) {
            skip_whitespace(parser);
        }
        
        // Check for end of array
        if (parser->position >= parser->length) {
            parser->error = "Unterminated array";
            parser->error_position = parser->position;
            break;
        }
        
        if (parser->input[parser->position] == ']') {
            parser->position++;
            break;
        }
        
        // Parse element
        mp_json_value_t *element = parse_value(parser, flags);
        if (!element) {
            // Free partially parsed array
            for (size_t i = 0; i < value->data.array.count; i++) {
                parser->free_fn(value->data.array.elements[i]);
            }
            parser->free_fn(value->data.array.elements);
            parser->free_fn(value);
            return NULL;
        }
        
        // Add element to array
        if (value->data.array.count >= value->data.array.capacity) {
            size_t new_capacity = value->data.array.capacity == 0 ? 8 : value->data.array.capacity * 2;
            mp_json_value_t **new_elements = (mp_json_value_t **)parser->alloc_fn(
                new_capacity * sizeof(mp_json_value_t *));
            
            if (!new_elements) {
                parser->error = "Out of memory";
                parser->error_position = parser->position;
                parser->free_fn(element);
                for (size_t i = 0; i < value->data.array.count; i++) {
                    parser->free_fn(value->data.array.elements[i]);
                }
                parser->free_fn(value->data.array.elements);
                parser->free_fn(value);
                return NULL;
            }
            
            for (size_t i = 0; i < value->data.array.count; i++) {
                new_elements[i] = value->data.array.elements[i];
            }
            
            parser->free_fn(value->data.array.elements);
            value->data.array.elements = new_elements;
            value->data.array.capacity = new_capacity;
        }
        
        value->data.array.elements[value->data.array.count] = element;
        value->data.array.count++;
        
        skip_whitespace(parser);
        
        // Check for comma or end
        if (parser->position < parser->length && parser->input[parser->position] == ',') {
            parser->position++;
            continue;
        } else if (parser->position < parser->length && parser->input[parser->position] == ']') {
            continue;
        } else if (flags & MP_JSON_PARSE_ALLOW_TRAILING_COMMA) {
            // Allow trailing comma
            if (parser->position < parser->length && parser->input[parser->position] == ']') {
                continue;
            }
        }
    }
    
    return value;
}

// Parse a JSON string
static mp_json_value_t *parse_string(mp_json_parser_t *parser, mp_json_parse_flags_t flags) {
    if (parser->position >= parser->length) {
        parser->error = "Unexpected end of input";
        parser->error_position = parser->position;
        return NULL;
    }
    
    char quote = parser->input[parser->position];
    
    // Check for single or double quotes
    if (quote != '"' && !(flags & MP_JSON_PARSE_ALLOW_SINGLE_QUOTES) || quote != '\'') {
        parser->error = "Expected string";
        parser->error_position = parser->position;
        return NULL;
    }
    
    parser->position++;
    
    // Find end of string
    size_t start = parser->position;
    while (parser->position < parser->length && parser->input[parser->position] != quote) {
        if (parser->input[parser->position] == '\\') {
            // Skip escaped character
            parser->position++;
            if (parser->position >= parser->length) {
                parser->error = "Unterminated string";
                parser->error_position = start;
                return NULL;
            }
        }
        parser->position++;
    }
    
    if (parser->position >= parser->length) {
        parser->error = "Unterminated string";
        parser->error_position = start;
        return NULL;
    }
    
    // Create string value
    mp_json_value_t *value = (mp_json_value_t *)parser->alloc_fn(sizeof(mp_json_value_t));
    if (!value) {
        parser->error = "Out of memory";
        parser->error_position = parser->position;
        return NULL;
    }
    
    value->type = MP_JSON_STRING;
    value->data.string.length = parser->position - start;
    value->data.string.owns_memory = true;
    
    // Allocate and copy string
    value->data.string.value = (char *)parser->alloc_fn(value->data.string.length + 1);
    if (!value->data.string.value) {
        parser->error = "Out of memory";
        parser->error_position = parser->position;
        parser->free_fn(value);
        return NULL;
    }
    
    // Copy and unescape string
    size_t out_pos = 0;
    for (size_t i = start; i < parser->position; i++) {
        if (parser->input[i] == '\\' && i + 1 < parser->position) {
            switch (parser->input[i + 1]) {
                case '"': value->data.string.value[out_pos++] = '"'; i++; break;
                case '\\': value->data.string.value[out_pos++] = '\\'; i++; break;
                case '/': value->data.string.value[out_pos++] = '/'; i++; break;
                case 'b': value->data.string.value[out_pos++] = '\b'; i++; break;
                case 'f': value->data.string.value[out_pos++] = '\f'; i++; break;
                case 'n': value->data.string.value[out_pos++] = '\n'; i++; break;
                case 'r': value->data.string.value[out_pos++] = '\r'; i++; break;
                case 't': value->data.string.value[out_pos++] = '\t'; i++; break;
                case 'u': {
                    // Unicode escape
                    if (i + 5 < parser->position) {
                        // For simplicity, just copy the escape sequence
                        value->data.string.value[out_pos++] = '\\';
                        value->data.string.value[out_pos++] = 'u';
                        for (int j = 0; j < 4; j++) {
                            value->data.string.value[out_pos++] = parser->input[i + 2 + j];
                        }
                        i += 5;
                    }
                    break;
                }
                default:
                    value->data.string.value[out_pos++] = parser->input[i + 1];
                    i++;
                    break;
            }
        } else {
            value->data.string.value[out_pos++] = parser->input[i];
        }
    }
    value->data.string.value[out_pos] = '\0';
    value->data.string.length = out_pos;
    
    // Skip closing quote
    parser->position++;
    
    return value;
}

// Parse a JSON number
static mp_json_value_t *parse_number(mp_json_parser_t *parser) {
    size_t start = parser->position;
    
    // Parse number
    while (parser->position < parser->length) {
        char c = parser->input[parser->position];
        if (is_digit(c) || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
            parser->position++;
        } else {
            break;
        }
    }
    
    // Parse the number
    size_t length = parser->position - start;
    char *num_str = (char *)parser->alloc_fn(length + 1);
    if (!num_str) {
        parser->error = "Out of memory";
        parser->error_position = start;
        return NULL;
    }
    
    memcpy(num_str, parser->input + start, length);
    num_str[length] = '\0';
    
    // Check if it's an integer
    bool is_integer = true;
    for (size_t i = 0; i < length; i++) {
        if (num_str[i] == '.' || num_str[i] == 'e' || num_str[i] == 'E') {
            is_integer = false;
            break;
        }
    }
    
    mp_json_value_t *value = (mp_json_value_t *)parser->alloc_fn(sizeof(mp_json_value_t));
    if (!value) {
        parser->free_fn(num_str);
        parser->error = "Out of memory";
        parser->error_position = start;
        return NULL;
    }
    
    value->type = MP_JSON_NUMBER;
    
    if (is_integer) {
        value->data.number.is_integer = true;
        value->data.number.integer_value = strtoll(num_str, NULL, 10);
        value->data.number.value = (double)value->data.number.integer_value;
    } else {
        value->data.number.is_integer = false;
        value->data.number.value = strtod(num_str, NULL);
        value->data.number.integer_value = (int64_t)value->data.number.value;
    }
    
    parser->free_fn(num_str);
    
    return value;
}

// Parse a JSON value
static mp_json_value_t *parse_value(mp_json_parser_t *parser, mp_json_parse_flags_t flags) {
    skip_whitespace(parser);
    
    // Check for comments
    while (skip_comments(parser, flags)) {
        skip_whitespace(parser);
    }
    
    if (parser->position >= parser->length) {
        parser->error = "Unexpected end of input";
        parser->error_position = parser->position;
        return NULL;
    }
    
    char c = parser->input[parser->position];
    
    switch (c) {
        case '{':
            return parse_object(parser, flags);
        case '[':
            return parse_array(parser, flags);
        case '"':
        case '\'':
            return parse_string(parser, flags);
        case 'n':
            // null
            if (parser->position + 3 < parser->length && 
                memcmp(parser->input + parser->position, "null", 4) == 0) {
                parser->position += 4;
                mp_json_value_t *value = (mp_json_value_t *)parser->alloc_fn(sizeof(mp_json_value_t));
                if (!value) {
                    parser->error = "Out of memory";
                    parser->error_position = parser->position;
                    return NULL;
                }
                value->type = MP_JSON_NULL;
                return value;
            }
            break;
        case 't':
            // true
            if (parser->position + 3 < parser->length && 
                memcmp(parser->input + parser->position, "true", 4) == 0) {
                parser->position += 4;
                mp_json_value_t *value = (mp_json_value_t *)parser->alloc_fn(sizeof(mp_json_value_t));
                if (!value) {
                    parser->error = "Out of memory";
                    parser->error_position = parser->position;
                    return NULL;
                }
                value->type = MP_JSON_BOOLEAN;
                value->data.boolean = true;
                return value;
            }
            break;
        case 'f':
            // false
            if (parser->position + 4 < parser->length && 
                memcmp(parser->input + parser->position, "false", 5) == 0) {
                parser->position += 5;
                mp_json_value_t *value = (mp_json_value_t *)parser->alloc_fn(sizeof(mp_json_value_t));
                if (!value) {
                    parser->error = "Out of memory";
                    parser->error_position = parser->position;
                    return NULL;
                }
                value->type = MP_JSON_BOOLEAN;
                value->data.boolean = false;
                return value;
            }
            break;
        case '-':
        case '+':
        case '.':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return parse_number(parser);
        default:
            // Check for unquoted keys (if allowed)
            if (flags & MP_JSON_PARSE_ALLOW_UNQUOTED_KEYS) {
                // For simplicity, treat as string
                return parse_string(parser, flags);
            }
            break;
    }
    
    parser->error = "Unexpected character";
    parser->error_position = parser->position;
    return NULL;
}

// Public API functions

int mp_json_parser_init(mp_json_parser_t *parser, const char *input) {
    return mp_json_parser_init_with_allocator(parser, input, default_alloc, default_free, NULL);
}

int mp_json_parser_init_with_allocator(mp_json_parser_t *parser, const char *input,
                                        void *(*alloc_fn)(size_t), void (*free_fn)(void *),
                                        void *alloc_data) {
    if (!parser) {
        return -1;
    }
    
    parser->input = input;
    parser->position = 0;
    parser->length = input ? strlen(input) : 0;
    parser->root = NULL;
    parser->error = NULL;
    parser->error_position = 0;
    parser->alloc_fn = alloc_fn ? alloc_fn : default_alloc;
    parser->free_fn = free_fn ? free_fn : default_free;
    parser->alloc_data = alloc_data;
    
    return 0;
}

int mp_json_parser_init_from_view(mp_json_parser_t *parser, mp_memory_view_t *view) {
    if (!parser || !view || !view->data) {
        return -1;
    }
    
    return mp_json_parser_init_with_allocator(parser, (const char *)view->data,
                                                default_alloc, default_free, NULL);
}

mp_json_value_t *mp_json_parse(mp_json_parser_t *parser, mp_json_parse_flags_t flags) {
    if (!parser) {
        return NULL;
    }
    
    parser->error = NULL;
    parser->error_position = 0;
    parser->position = 0;
    
    mp_json_value_t *value = parse_value(parser, flags);
    
    if (!value) {
        return NULL;
    }
    
    // Skip any trailing whitespace/comments
    skip_whitespace(parser);
    while (skip_comments(parser, flags)) {
        skip_whitespace(parser);
    }
    
    // Check if we consumed all input
    if (parser->position < parser->length) {
        parser->error = "Unexpected trailing data";
        parser->error_position = parser->position;
        mp_json_free(value);
        return NULL;
    }
    
    parser->root = value;
    return value;
}

void mp_json_free(mp_json_value_t *value) {
    if (!value) {
        return;
    }
    
    switch (value->type) {
        case MP_JSON_STRING:
            if (value->data.string.owns_memory && value->data.string.value) {
                free(value->data.string.value);
            }
            break;
        case MP_JSON_OBJECT:
            for (size_t i = 0; i < value->data.object.count; i++) {
                mp_json_free(value->data.object.keys[i]);
                mp_json_free(value->data.object.values[i]);
            }
            if (value->data.object.keys) {
                free(value->data.object.keys);
            }
            if (value->data.object.values) {
                free(value->data.object.values);
            }
            break;
        case MP_JSON_ARRAY:
            for (size_t i = 0; i < value->data.array.count; i++) {
                mp_json_free(value->data.array.elements[i]);
            }
            if (value->data.array.elements) {
                free(value->data.array.elements);
            }
            break;
        default:
            break;
    }
    
    free(value);
}

void mp_json_parser_destroy(mp_json_parser_t *parser) {
    if (!parser) {
        return;
    }
    
    if (parser->root) {
        mp_json_free(parser->root);
        parser->root = NULL;
    }
}

mp_json_value_t *mp_json_parser_get_root(mp_json_parser_t *parser) {
    if (!parser) {
        return NULL;
    }
    return parser->root;
}

const char *mp_json_parser_get_error(mp_json_parser_t *parser) {
    if (!parser) {
        return NULL;
    }
    return parser->error;
}

size_t mp_json_parser_get_error_position(mp_json_parser_t *parser) {
    if (!parser) {
        return 0;
    }
    return parser->error_position;
}

// JSON generation functions

int mp_json_generator_init(mp_json_generator_t *generator, char *buffer, size_t capacity) {
    if (!generator) {
        return -1;
    }
    
    generator->buffer = buffer;
    generator->position = 0;
    generator->capacity = capacity;
    generator->pretty_print = false;
    generator->indent_level = 0;
    generator->realloc_fn = NULL;
    generator->alloc_data = NULL;
    
    if (buffer && capacity > 0) {
        buffer[0] = '\0';
    }
    
    return 0;
}

int mp_json_generator_init_with_allocator(mp_json_generator_t *generator, size_t capacity,
                                           void *(*realloc_fn)(void *, size_t), void *alloc_data) {
    if (!generator) {
        return -1;
    }
    
    generator->buffer = NULL;
    generator->position = 0;
    generator->capacity = 0;
    generator->pretty_print = false;
    generator->indent_level = 0;
    generator->realloc_fn = realloc_fn ? realloc_fn : default_realloc;
    generator->alloc_data = alloc_data;
    
    // Allocate initial buffer
    if (capacity > 0) {
        generator->buffer = (char *)generator->realloc_fn(NULL, capacity);
        if (!generator->buffer) {
            return -1;
        }
        generator->capacity = capacity;
        generator->buffer[0] = '\0';
    }
    
    return 0;
}

void mp_json_generator_destroy(mp_json_generator_t *generator) {
    if (!generator) {
        return;
    }
    
    if (generator->buffer && generator->realloc_fn) {
        generator->realloc_fn(generator->buffer, 0);
    }
    
    generator->buffer = NULL;
    generator->position = 0;
    generator->capacity = 0;
}

void mp_json_generator_set_flags(mp_json_generator_t *generator, mp_json_gen_flags_t flags) {
    if (!generator) {
        return;
    }
    generator->pretty_print = (flags & MP_JSON_GEN_PRETTY) != 0;
}

void mp_json_generator_set_indent(mp_json_generator_t *generator, int indent) {
    if (!generator) {
        return;
    }
    generator->indent_level = indent;
}

static void ensure_capacity(mp_json_generator_t *generator, size_t needed) {
    if (!generator) {
        return;
    }
    
    if (generator->position + needed < generator->capacity) {
        return;
    }
    
    // Need to reallocate
    size_t new_capacity = generator->capacity == 0 ? 256 : generator->capacity * 2;
    while (generator->position + needed >= new_capacity) {
        new_capacity *= 2;
    }
    
    if (generator->realloc_fn) {
        char *new_buffer = (char *)generator->realloc_fn(generator->buffer, new_capacity);
        if (new_buffer) {
            generator->buffer = new_buffer;
            generator->capacity = new_capacity;
        }
    }
}

static void write_char(mp_json_generator_t *generator, char c) {
    ensure_capacity(generator, 1);
    if (generator->position < generator->capacity) {
        generator->buffer[generator->position++] = c;
        generator->buffer[generator->position] = '\0';
    }
}

static void write_string(mp_json_generator_t *generator, const char *str) {
    if (!str) {
        return;
    }
    
    size_t len = strlen(str);
    ensure_capacity(generator, len);
    
    if (generator->position + len < generator->capacity) {
        memcpy(generator->buffer + generator->position, str, len);
        generator->position += len;
        generator->buffer[generator->position] = '\0';
    }
}

static void write_indent(mp_json_generator_t *generator) {
    if (!generator->pretty_print) {
        return;
    }
    
    for (int i = 0; i < generator->indent_level; i++) {
        write_string(generator, "  ");
    }
}

static void write_newline(mp_json_generator_t *generator) {
    if (generator->pretty_print) {
        write_char(generator, '\n');
    }
}

static void write_escaped_string(mp_json_generator_t *generator, const char *str, size_t len) {
    if (!str) {
        write_string(generator, "null");
        return;
    }
    
    write_char(generator, '"');
    
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        
        switch (c) {
            case '"': write_string(generator, "\\\""); break;
            case '\\': write_string(generator, "\\\\"); break;
            case '\b': write_string(generator, "\\b"); break;
            case '\f': write_string(generator, "\\f"); break;
            case '\n': write_string(generator, "\\n"); break;
            case '\r': write_string(generator, "\\r"); break;
            case '\t': write_string(generator, "\\t"); break;
            default:
                if (c >= 0 && c < 32) {
                    // Control character - escape as unicode
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    write_string(generator, buf);
                } else {
                    write_char(generator, c);
                }
                break;
        }
    }
    
    write_char(generator, '"');
}

static int generate_value(mp_json_generator_t *generator, mp_json_value_t *value);

static int generate_object(mp_json_generator_t *generator, mp_json_object_t *object) {
    write_char(generator, '{');
    
    if (generator->pretty_print) {
        write_newline(generator);
        generator->indent_level++;
    }
    
    for (size_t i = 0; i < object->count; i++) {
        if (i > 0) {
            write_char(generator, ',');
            if (generator->pretty_print) {
                write_newline(generator);
            }
        }
        
        if (generator->pretty_print) {
            write_indent(generator);
        }
        
        // Write key
        mp_json_value_t *key = object->keys[i];
        if (key->type == MP_JSON_STRING) {
            write_escaped_string(generator, key->data.string.value, key->data.string.length);
        } else {
            // Non-string key - convert to string
            char *key_str = mp_json_to_string(key, 0);
            write_escaped_string(generator, key_str, strlen(key_str));
            free(key_str);
        }
        
        write_char(generator, ':');
        
        if (generator->pretty_print) {
            write_char(generator, ' ');
        }
        
        // Write value
        if (generate_value(generator, object->values[i]) != 0) {
            return -1;
        }
    }
    
    if (generator->pretty_print) {
        write_newline(generator);
        generator->indent_level--;
        write_indent(generator);
    }
    
    write_char(generator, '}');
    
    return 0;
}

static int generate_array(mp_json_generator_t *generator, mp_json_array_t *array) {
    write_char(generator, '[');
    
    if (generator->pretty_print) {
        write_newline(generator);
        generator->indent_level++;
    }
    
    for (size_t i = 0; i < array->count; i++) {
        if (i > 0) {
            write_char(generator, ',');
            if (generator->pretty_print) {
                write_newline(generator);
            }
        }
        
        if (generator->pretty_print) {
            write_indent(generator);
        }
        
        if (generate_value(generator, array->elements[i]) != 0) {
            return -1;
        }
    }
    
    if (generator->pretty_print) {
        write_newline(generator);
        generator->indent_level--;
        write_indent(generator);
    }
    
    write_char(generator, ']');
    
    return 0;
}

static int generate_value(mp_json_generator_t *generator, mp_json_value_t *value) {
    if (!value) {
        write_string(generator, "null");
        return 0;
    }
    
    switch (value->type) {
        case MP_JSON_NULL:
            write_string(generator, "null");
            break;
        case MP_JSON_BOOLEAN:
            write_string(generator, value->data.boolean ? "true" : "false");
            break;
        case MP_JSON_NUMBER:
            if (value->data.number.is_integer) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%lld", (long long)value->data.number.integer_value);
                write_string(generator, buf);
            } else {
                // Check if we can print as integer
                if (value->data.number.value == (double)value->data.number.integer_value) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%lld", (long long)value->data.number.integer_value);
                    write_string(generator, buf);
                } else {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%.15g", value->data.number.value);
                    write_string(generator, buf);
                }
            }
            break;
        case MP_JSON_STRING:
            write_escaped_string(generator, value->data.string.value, value->data.string.length);
            break;
        case MP_JSON_OBJECT:
            if (generate_object(generator, &value->data.object) != 0) {
                return -1;
            }
            break;
        case MP_JSON_ARRAY:
            if (generate_array(generator, &value->data.array) != 0) {
                return -1;
            }
            break;
        case MP_JSON_ERROR:
            write_string(generator, "null");
            break;
    }
    
    return 0;
}

int mp_json_generate(mp_json_generator_t *generator, mp_json_value_t *value) {
    if (!generator) {
        return -1;
    }
    
    generator->position = 0;
    if (generator->buffer && generator->capacity > 0) {
        generator->buffer[0] = '\0';
    }
    
    return generate_value(generator, value);
}

const char *mp_json_generator_get_output(mp_json_generator_t *generator) {
    if (!generator) {
        return NULL;
    }
    return generator->buffer;
}

size_t mp_json_generator_get_length(mp_json_generator_t *generator) {
    if (!generator) {
        return 0;
    }
    return generator->position;
}

void mp_json_generator_reset(mp_json_generator_t *generator) {
    if (!generator) {
        return;
    }
    generator->position = 0;
    if (generator->buffer && generator->capacity > 0) {
        generator->buffer[0] = '\0';
    }
}

// Value creation functions

mp_json_value_t *mp_json_null(void) {
    mp_json_value_t *value = (mp_json_value_t *)malloc(sizeof(mp_json_value_t));
    if (!value) {
        return NULL;
    }
    value->type = MP_JSON_NULL;
    return value;
}

mp_json_value_t *mp_json_boolean(bool val) {
    mp_json_value_t *value = (mp_json_value_t *)malloc(sizeof(mp_json_value_t));
    if (!value) {
        return NULL;
    }
    value->type = MP_JSON_BOOLEAN;
    value->data.boolean = val;
    return value;
}

mp_json_value_t *mp_json_number(double val) {
    mp_json_value_t *value = (mp_json_value_t *)malloc(sizeof(mp_json_value_t));
    if (!value) {
        return NULL;
    }
    value->type = MP_JSON_NUMBER;
    value->data.number.value = val;
    value->data.number.is_integer = false;
    value->data.number.integer_value = (int64_t)val;
    return value;
}

mp_json_value_t *mp_json_integer(int64_t val) {
    mp_json_value_t *value = (mp_json_value_t *)malloc(sizeof(mp_json_value_t));
    if (!value) {
        return NULL;
    }
    value->type = MP_JSON_NUMBER;
    value->data.number.value = (double)val;
    value->data.number.is_integer = true;
    value->data.number.integer_value = val;
    return value;
}

mp_json_value_t *mp_json_string(const char *val) {
    if (!val) {
        return mp_json_null();
    }
    return mp_json_string_with_length(val, strlen(val));
}

mp_json_value_t *mp_json_string_with_length(const char *val, size_t len) {
    mp_json_value_t *value = (mp_json_value_t *)malloc(sizeof(mp_json_value_t));
    if (!value) {
        return NULL;
    }
    
    value->type = MP_JSON_STRING;
    value->data.string.length = len;
    value->data.string.owns_memory = true;
    value->data.string.value = (char *)malloc(len + 1);
    
    if (!value->data.string.value) {
        free(value);
        return NULL;
    }
    
    memcpy(value->data.string.value, val, len);
    value->data.string.value[len] = '\0';
    
    return value;
}

mp_json_value_t *mp_json_string_owning(char *val, size_t len) {
    mp_json_value_t *value = (mp_json_value_t *)malloc(sizeof(mp_json_value_t));
    if (!value) {
        return NULL;
    }
    
    value->type = MP_JSON_STRING;
    value->data.string.value = val;
    value->data.string.length = len;
    value->data.string.owns_memory = true;
    
    return value;
}

mp_json_value_t *mp_json_object(void) {
    mp_json_value_t *value = (mp_json_value_t *)malloc(sizeof(mp_json_value_t));
    if (!value) {
        return NULL;
    }
    value->type = MP_JSON_OBJECT;
    value->data.object.keys = NULL;
    value->data.object.values = NULL;
    value->data.object.count = 0;
    value->data.object.capacity = 0;
    return value;
}

mp_json_value_t *mp_json_array(void) {
    mp_json_value_t *value = (mp_json_value_t *)malloc(sizeof(mp_json_value_t));
    if (!value) {
        return NULL;
    }
    value->type = MP_JSON_ARRAY;
    value->data.array.elements = NULL;
    value->data.array.count = 0;
    value->data.array.capacity = 0;
    return value;
}

int mp_json_object_add(mp_json_value_t *object, const char *key, mp_json_value_t *value) {
    if (!object || object->type != MP_JSON_OBJECT || !key) {
        return -1;
    }
    
    mp_json_value_t *key_value = mp_json_string(key);
    if (!key_value) {
        return -1;
    }
    
    return mp_json_object_add_owning(object, key, value);
}

int mp_json_object_add_owning(mp_json_value_t *object, const char *key, mp_json_value_t *value) {
    if (!object || object->type != MP_JSON_OBJECT || !value) {
        if (value) mp_json_free(value);
        return -1;
    }
    
    // Check if we need to resize
    if (object->data.object.count >= object->data.object.capacity) {
        size_t new_capacity = object->data.object.capacity == 0 ? 8 : object->data.object.capacity * 2;
        mp_json_value_t **new_keys = (mp_json_value_t **)realloc(
            object->data.object.keys, new_capacity * sizeof(mp_json_value_t *));
        mp_json_value_t **new_values = (mp_json_value_t **)realloc(
            object->data.object.values, new_capacity * sizeof(mp_json_value_t *));
        
        if (!new_keys || !new_values) {
            if (new_keys) free(new_keys);
            if (new_values) free(new_values);
            mp_json_free(value);
            return -1;
        }
        
        object->data.object.keys = new_keys;
        object->data.object.values = new_values;
        object->data.object.capacity = new_capacity;
    }
    
    // Create key value
    mp_json_value_t *key_value = mp_json_string(key);
    if (!key_value) {
        mp_json_free(value);
        return -1;
    }
    
    object->data.object.keys[object->data.object.count] = key_value;
    object->data.object.values[object->data.object.count] = value;
    object->data.object.count++;
    
    return 0;
}

mp_json_value_t *mp_json_object_get(mp_json_value_t *object, const char *key) {
    if (!object || object->type != MP_JSON_OBJECT || !key) {
        return NULL;
    }
    
    for (size_t i = 0; i < object->data.object.count; i++) {
        mp_json_value_t *k = object->data.object.keys[i];
        if (k->type == MP_JSON_STRING && strcmp(k->data.string.value, key) == 0) {
            return object->data.object.values[i];
        }
    }
    
    return NULL;
}

bool mp_json_object_has(mp_json_value_t *object, const char *key) {
    return mp_json_object_get(object, key) != NULL;
}

mp_json_value_t *mp_json_object_remove(mp_json_value_t *object, const char *key) {
    if (!object || object->type != MP_JSON_OBJECT || !key) {
        return NULL;
    }
    
    for (size_t i = 0; i < object->data.object.count; i++) {
        mp_json_value_t *k = object->data.object.keys[i];
        if (k->type == MP_JSON_STRING && strcmp(k->data.string.value, key) == 0) {
            mp_json_value_t *value = object->data.object.values[i];
            
            // Remove from array
            for (size_t j = i; j < object->data.object.count - 1; j++) {
                object->data.object.keys[j] = object->data.object.keys[j + 1];
                object->data.object.values[j] = object->data.object.values[j + 1];
            }
            
            object->data.object.count--;
            
            // Free the key
            mp_json_free(k);
            
            return value;
        }
    }
    
    return NULL;
}

size_t mp_json_object_size(mp_json_value_t *object) {
    if (!object || object->type != MP_JSON_OBJECT) {
        return 0;
    }
    return object->data.object.count;
}

const char **mp_json_object_keys(mp_json_value_t *object) {
    if (!object || object->type != MP_JSON_OBJECT) {
        return NULL;
    }
    
    const char **keys = (const char **)malloc(object->data.object.count * sizeof(const char *));
    if (!keys) {
        return NULL;
    }
    
    for (size_t i = 0; i < object->data.object.count; i++) {
        keys[i] = object->data.object.keys[i]->data.string.value;
    }
    
    return keys;
}

int mp_json_array_add(mp_json_value_t *array, mp_json_value_t *value) {
    if (!array || array->type != MP_JSON_ARRAY || !value) {
        return -1;
    }
    
    // Check if we need to resize
    if (array->data.array.count >= array->data.array.capacity) {
        size_t new_capacity = array->data.array.capacity == 0 ? 8 : array->data.array.capacity * 2;
        mp_json_value_t **new_elements = (mp_json_value_t **)realloc(
            array->data.array.elements, new_capacity * sizeof(mp_json_value_t *));
        
        if (!new_elements) {
            return -1;
        }
        
        array->data.array.elements = new_elements;
        array->data.array.capacity = new_capacity;
    }
    
    array->data.array.elements[array->data.array.count] = value;
    array->data.array.count++;
    
    return 0;
}

mp_json_value_t *mp_json_array_get(mp_json_value_t *array, size_t index) {
    if (!array || array->type != MP_JSON_ARRAY || index >= array->data.array.count) {
        return NULL;
    }
    return array->data.array.elements[index];
}

size_t mp_json_array_size(mp_json_value_t *array) {
    if (!array || array->type != MP_JSON_ARRAY) {
        return 0;
    }
    return array->data.array.count;
}

mp_json_type_t mp_json_get_type(mp_json_value_t *value) {
    if (!value) {
        return MP_JSON_NULL;
    }
    return value->type;
}

bool mp_json_get_boolean(mp_json_value_t *value) {
    if (!value || value->type != MP_JSON_BOOLEAN) {
        return false;
    }
    return value->data.boolean;
}

double mp_json_get_number(mp_json_value_t *value) {
    if (!value || value->type != MP_JSON_NUMBER) {
        return 0.0;
    }
    return value->data.number.value;
}

int64_t mp_json_get_integer(mp_json_value_t *value) {
    if (!value || value->type != MP_JSON_NUMBER) {
        return 0;
    }
    return value->data.number.integer_value;
}

const char *mp_json_get_string(mp_json_value_t *value) {
    if (!value || value->type != MP_JSON_STRING) {
        return NULL;
    }
    return value->data.string.value;
}

size_t mp_json_get_string_length(mp_json_value_t *value) {
    if (!value || value->type != MP_JSON_STRING) {
        return 0;
    }
    return value->data.string.length;
}

bool mp_json_is_null(mp_json_value_t *value) {
    return value != NULL && value->type == MP_JSON_NULL;
}

bool mp_json_is_boolean(mp_json_value_t *value) {
    return value != NULL && value->type == MP_JSON_BOOLEAN;
}

bool mp_json_is_number(mp_json_value_t *value) {
    return value != NULL && value->type == MP_JSON_NUMBER;
}

bool mp_json_is_string(mp_json_value_t *value) {
    return value != NULL && value->type == MP_JSON_STRING;
}

bool mp_json_is_object(mp_json_value_t *value) {
    return value != NULL && value->type == MP_JSON_OBJECT;
}

bool mp_json_is_array(mp_json_value_t *value) {
    return value != NULL && value->type == MP_JSON_ARRAY;
}

mp_json_value_t *mp_json_deep_copy(mp_json_value_t *value) {
    if (!value) {
        return NULL;
    }
    
    switch (value->type) {
        case MP_JSON_NULL:
            return mp_json_null();
        case MP_JSON_BOOLEAN:
            return mp_json_boolean(value->data.boolean);
        case MP_JSON_NUMBER:
            if (value->data.number.is_integer) {
                return mp_json_integer(value->data.number.integer_value);
            } else {
                return mp_json_number(value->data.number.value);
            }
        case MP_JSON_STRING:
            return mp_json_string_with_length(value->data.string.value, value->data.string.length);
        case MP_JSON_OBJECT: {
            mp_json_value_t *copy = mp_json_object();
            if (!copy) {
                return NULL;
            }
            for (size_t i = 0; i < value->data.object.count; i++) {
                mp_json_value_t *key_copy = mp_json_deep_copy(value->data.object.keys[i]);
                mp_json_value_t *val_copy = mp_json_deep_copy(value->data.object.values[i]);
                if (!key_copy || !val_copy) {
                    mp_json_free(copy);
                    if (key_copy) mp_json_free(key_copy);
                    if (val_copy) mp_json_free(val_copy);
                    return NULL;
                }
                if (mp_json_object_add_owning(copy, key_copy->data.string.value, val_copy) != 0) {
                    mp_json_free(copy);
                    mp_json_free(key_copy);
                    mp_json_free(val_copy);
                    return NULL;
                }
            }
            return copy;
        }
        case MP_JSON_ARRAY: {
            mp_json_value_t *copy = mp_json_array();
            if (!copy) {
                return NULL;
            }
            for (size_t i = 0; i < value->data.array.count; i++) {
                mp_json_value_t *val_copy = mp_json_deep_copy(value->data.array.elements[i]);
                if (!val_copy) {
                    mp_json_free(copy);
                    return NULL;
                }
                if (mp_json_array_add(copy, val_copy) != 0) {
                    mp_json_free(copy);
                    mp_json_free(val_copy);
                    return NULL;
                }
            }
            return copy;
        }
        case MP_JSON_ERROR:
            return NULL;
    }
    
    return NULL;
}

bool mp_json_equal(mp_json_value_t *a, mp_json_value_t *b) {
    if (!a || !b) {
        return a == b;
    }
    
    if (a->type != b->type) {
        return false;
    }
    
    switch (a->type) {
        case MP_JSON_NULL:
            return true;
        case MP_JSON_BOOLEAN:
            return a->data.boolean == b->data.boolean;
        case MP_JSON_NUMBER:
            if (a->data.number.is_integer && b->data.number.is_integer) {
                return a->data.number.integer_value == b->data.number.integer_value;
            } else {
                return a->data.number.value == b->data.number.value;
            }
        case MP_JSON_STRING:
            if (a->data.string.length != b->data.string.length) {
                return false;
            }
            return memcmp(a->data.string.value, b->data.string.value, a->data.string.length) == 0;
        case MP_JSON_OBJECT:
            if (a->data.object.count != b->data.object.count) {
                return false;
            }
            for (size_t i = 0; i < a->data.object.count; i++) {
                mp_json_value_t *a_key = a->data.object.keys[i];
                mp_json_value_t *a_val = a->data.object.values[i];
                
                mp_json_value_t *b_val = mp_json_object_get(b, a_key->data.string.value);
                if (!b_val) {
                    return false;
                }
                
                if (!mp_json_equal(a_val, b_val)) {
                    return false;
                }
            }
            return true;
        case MP_JSON_ARRAY:
            if (a->data.array.count != b->data.array.count) {
                return false;
            }
            for (size_t i = 0; i < a->data.array.count; i++) {
                if (!mp_json_equal(a->data.array.elements[i], b->data.array.elements[i])) {
                    return false;
                }
            }
            return true;
        case MP_JSON_ERROR:
            return false;
    }
    
    return false;
}

char *mp_json_to_string(mp_json_value_t *value, mp_json_gen_flags_t flags) {
    mp_json_generator_t generator;
    
    if (mp_json_generator_init_with_allocator(&generator, 256, default_realloc, NULL) != 0) {
        return NULL;
    }
    
    generator.pretty_print = (flags & MP_JSON_GEN_PRETTY) != 0;
    
    if (mp_json_generate(&generator, value) != 0) {
        mp_json_generator_destroy(&generator);
        return NULL;
    }
    
    char *result = strdup(generator.buffer);
    mp_json_generator_destroy(&generator);
    
    return result;
}

mp_json_value_t *mp_json_parse_file(const char *filename, mp_json_parse_flags_t flags) {
    (void)filename;
    (void)flags;
    // TODO: Implement file parsing
    return NULL;
}

int mp_json_write_file(const char *filename, mp_json_value_t *value, mp_json_gen_flags_t flags) {
    (void)filename;
    (void)value;
    (void)flags;
    // TODO: Implement file writing
    return -1;
}
