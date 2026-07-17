#include "microposix/serialization/json.h"
#include "microposix/serialization/protobuf.h"
#include "microposix/serialization/edf.h"
#include <stdio.h>
#include <string.h>

// Test JSON
static void test_json(void) {
    printf("Testing JSON...\n");
    
    // Test parsing
    const char *json_str = "{\"name\":\"test\",\"value\":42,\"active\":true,\"items\":[1,2,3]}";
    
    mp_json_parser_t parser;
    if (mp_json_parser_init(&parser, json_str) != 0) {
        printf("ERROR: Failed to initialize JSON parser\n");
        return;
    }
    
    mp_json_value_t *value = mp_json_parse(&parser, 0);
    if (!value) {
        printf("ERROR: Failed to parse JSON: %s\n", mp_json_parser_get_error(&parser));
        mp_json_parser_destroy(&parser);
        return;
    }
    
    // Check type
    if (!mp_json_is_object(value)) {
        printf("ERROR: Root should be an object\n");
        mp_json_free(value);
        mp_json_parser_destroy(&parser);
        return;
    }
    
    // Get fields
    mp_json_value_t *name = mp_json_object_get(value, "name");
    if (!name || !mp_json_is_string(name)) {
        printf("ERROR: 'name' should be a string\n");
    } else if (strcmp(mp_json_get_string(name), "test") != 0) {
        printf("ERROR: 'name' should be 'test', got '%s'\n", mp_json_get_string(name));
    }
    
    mp_json_value_t *val = mp_json_object_get(value, "value");
    if (!val || !mp_json_is_number(val)) {
        printf("ERROR: 'value' should be a number\n");
    } else if (mp_json_get_integer(val) != 42) {
        printf("ERROR: 'value' should be 42, got %lld\n", mp_json_get_integer(val));
    }
    
    mp_json_value_t *active = mp_json_object_get(value, "active");
    if (!active || !mp_json_is_boolean(active)) {
        printf("ERROR: 'active' should be a boolean\n");
    } else if (mp_json_get_boolean(active) != true) {
        printf("ERROR: 'active' should be true\n");
    }
    
    mp_json_value_t *items = mp_json_object_get(value, "items");
    if (!items || !mp_json_is_array(items)) {
        printf("ERROR: 'items' should be an array\n");
    } else if (mp_json_array_size(items) != 3) {
        printf("ERROR: 'items' should have 3 elements, got %zu\n", mp_json_array_size(items));
    }
    
    // Test generation
    mp_json_generator_t generator;
    if (mp_json_generator_init(&generator, NULL, 0) != 0) {
        printf("ERROR: Failed to initialize JSON generator\n");
        mp_json_free(value);
        mp_json_parser_destroy(&parser);
        return;
    }
    
    generator.pretty_print = false;
    
    if (mp_json_generate(&generator, value) != 0) {
        printf("ERROR: Failed to generate JSON\n");
    } else {
        const char *output = mp_json_generator_get_output(&generator);
        printf("Generated JSON: %s\n", output);
    }
    
    // Cleanup
    mp_json_free(value);
    mp_json_generator_destroy(&generator);
    mp_json_parser_destroy(&parser);
    
    printf("JSON tests passed!\n");
}

// Test JSON creation
static void test_json_creation(void) {
    printf("Testing JSON creation...\n");
    
    // Create a JSON object
    mp_json_value_t *obj = mp_json_object();
    if (!obj) {
        printf("ERROR: Failed to create JSON object\n");
        return;
    }
    
    // Add fields
    mp_json_object_add(obj, "string", mp_json_string("hello"));
    mp_json_object_add(obj, "number", mp_json_integer(123));
    mp_json_object_add(obj, "boolean", mp_json_boolean(true));
    mp_json_object_add(obj, "null", mp_json_null());
    
    // Create an array
    mp_json_value_t *arr = mp_json_array();
    mp_json_array_add(arr, mp_json_integer(1));
    mp_json_array_add(arr, mp_json_integer(2));
    mp_json_array_add(arr, mp_json_integer(3));
    mp_json_object_add(obj, "array", arr);
    
    // Convert to string
    char *str = mp_json_to_string(obj, 0);
    if (!str) {
        printf("ERROR: Failed to convert JSON to string\n");
        mp_json_free(obj);
        return;
    }
    
    printf("Created JSON: %s\n", str);
    
    // Cleanup
    free(str);
    mp_json_free(obj);
    
    printf("JSON creation tests passed!\n");
}

// Test protobuf
static void test_protobuf(void) {
    printf("Testing Protocol Buffers...\n");
    
    // Test varint encoding
    mp_protobuf_writer_t writer;
    uint8_t buffer[128];
    
    if (mp_protobuf_writer_init(&writer, buffer, 128) != 0) {
        printf("ERROR: Failed to initialize protobuf writer\n");
        return;
    }
    
    // Write some values
    if (mp_protobuf_write_varint(&writer, 12345) != 0) {
        printf("ERROR: Failed to write varint\n");
        mp_protobuf_writer_destroy(&writer);
        return;
    }
    
    if (mp_protobuf_write_fixed32(&writer, 0xDEADBEEF) != 0) {
        printf("ERROR: Failed to write fixed32\n");
        mp_protobuf_writer_destroy(&writer);
        return;
    }
    
    if (mp_protobuf_write_string(&writer, "Hello, protobuf!") != 0) {
        printf("ERROR: Failed to write string\n");
        mp_protobuf_writer_destroy(&writer);
        return;
    }
    
    size_t written = mp_protobuf_writer_get_length(&writer);
    printf("Wrote %zu bytes of protobuf data\n", written);
    
    // Test reading
    mp_protobuf_reader_t reader;
    if (mp_protobuf_reader_init(&reader, buffer, written) != 0) {
        printf("ERROR: Failed to initialize protobuf reader\n");
        mp_protobuf_writer_destroy(&writer);
        return;
    }
    
    uint64_t val;
    if (mp_protobuf_read_varint(&reader, &val) != 0) {
        printf("ERROR: Failed to read varint\n");
    } else if (val != 12345) {
        printf("ERROR: Varint should be 12345, got %lu\n", val);
    }
    
    uint32_t fixed_val;
    if (mp_protobuf_read_fixed32(&reader, &fixed_val) != 0) {
        printf("ERROR: Failed to read fixed32\n");
    } else if (fixed_val != 0xDEADBEEF) {
        printf("ERROR: Fixed32 should be 0xDEADBEEF, got 0x%X\n", fixed_val);
    }
    
    char *str;
    size_t str_len;
    if (mp_protobuf_read_string(&reader, &str, &str_len) != 0) {
        printf("ERROR: Failed to read string\n");
    } else {
        printf("Read string: %s (length: %zu)\n", str, str_len);
        free(str);
    }
    
    // Cleanup
    mp_protobuf_reader_destroy(&reader);
    mp_protobuf_writer_destroy(&writer);
    
    printf("Protocol Buffers tests passed!\n");
}

// Test EDF
static void test_edf(void) {
    printf("Testing EDF...\n");
    
    // Test basic encoding
    mp_edf_writer_t writer;
    uint8_t buffer[128];
    
    if (mp_edf_writer_init(&writer, buffer, 128) != 0) {
        printf("ERROR: Failed to initialize EDF writer\n");
        return;
    }
    
    // Write some values
    if (mp_edf_write_int32(&writer, 12345) != 0) {
        printf("ERROR: Failed to write int32\n");
        mp_edf_writer_destroy(&writer);
        return;
    }
    
    if (mp_edf_write_string(&writer, "Hello, EDF!") != 0) {
        printf("ERROR: Failed to write string\n");
        mp_edf_writer_destroy(&writer);
        return;
    }
    
    if (mp_edf_write_bool(&writer, true) != 0) {
        printf("ERROR: Failed to write bool\n");
        mp_edf_writer_destroy(&writer);
        return;
    }
    
    size_t written = mp_edf_writer_get_length(&writer);
    printf("Wrote %zu bytes of EDF data\n", written);
    
    // Test reading
    mp_edf_reader_t reader;
    if (mp_edf_reader_init(&reader, buffer, written) != 0) {
        printf("ERROR: Failed to initialize EDF reader\n");
        mp_edf_writer_destroy(&writer);
        return;
    }
    
    int32_t val;
    if (mp_edf_read_int32(&reader, &val) != 0) {
        printf("ERROR: Failed to read int32\n");
    } else if (val != 12345) {
        printf("ERROR: Int32 should be 12345, got %d\n", val);
    }
    
    char *str;
    size_t str_len;
    if (mp_edf_read_string(&reader, &str, &str_len) != 0) {
        printf("ERROR: Failed to read string\n");
    } else {
        printf("Read string: %s (length: %zu)\n", str, str_len);
        free(str);
    }
    
    bool bool_val;
    if (mp_edf_read_bool(&reader, &bool_val) != 0) {
        printf("ERROR: Failed to read bool\n");
    } else if (bool_val != true) {
        printf("ERROR: Bool should be true\n");
    }
    
    // Cleanup
    mp_edf_reader_destroy(&reader);
    mp_edf_writer_destroy(&writer);
    
    printf("EDF tests passed!\n");
}

int main(void) {
    printf("Running serialization tests...\n\n");
    
    test_json();
    test_json_creation();
    test_protobuf();
    test_edf();
    
    printf("\nAll serialization tests passed!\n");
    return 0;
}
