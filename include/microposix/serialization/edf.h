#ifndef MICROPOSIX_EDF_H
#define MICROPOSIX_EDF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "microposix/mm/memory_view.h"

/**
 * @file edf.h
 * @brief Extensible Data Format (EDF) serialization
 * 
 * EDF is a binary serialization format designed for embedded systems.
 * It provides:
 * - Compact binary representation
 * - Type-safe encoding/decoding
 * - Support for nested structures
 * - Zero-copy reading where possible
 * - Schema evolution support
 * 
 * EDF is similar to Protocol Buffers but optimized for microcontroller use.
 */

// Forward declarations
typedef struct mp_edf_writer mp_edf_writer_t;
typedef struct mp_edf_reader mp_edf_reader_t;
typedef enum mp_edf_type mp_edf_type_t;
typedef struct mp_edf_field mp_edf_field_t;
typedef struct mp_edf_schema mp_edf_schema_t;

/**
 * @brief EDF data types
 */
typedef enum {
    MP_EDF_NULL = 0,
    MP_EDF_BOOL = 1,
    MP_EDF_INT8 = 2,
    MP_EDF_INT16 = 3,
    MP_EDF_INT32 = 4,
    MP_EDF_INT64 = 5,
    MP_EDF_UINT8 = 6,
    MP_EDF_UINT16 = 7,
    MP_EDF_UINT32 = 8,
    MP_EDF_UINT64 = 9,
    MP_EDF_FLOAT = 10,
    MP_EDF_DOUBLE = 11,
    MP_EDF_STRING = 12,
    MP_EDF_BYTES = 13,
    MP_EDF_ARRAY = 14,
    MP_EDF_OBJECT = 15,
    MP_EDF_ENUM = 16,
    MP_EDF_FIXED32 = 17,
    MP_EDF_FIXED64 = 18,
    MP_EDF_SFIXED32 = 19,
    MP_EDF_SFIXED64 = 20,
    MP_EDF_SINT32 = 21,
    MP_EDF_SINT64 = 22
} mp_edf_type_t;

/**
 * @brief EDF field descriptor
 */
struct mp_edf_field {
    const char *name;            ///< Field name
    uint32_t id;                ///< Field ID
    mp_edf_type_t type;         ///< Field type
    bool required;              ///< Whether field is required
    bool repeated;              ///< Whether field is repeated
    const char *type_name;      ///< Type name for complex types
    size_t offset;             ///< Offset in struct
    size_t size;               ///< Size of field in struct
};

/**
 * @brief EDF schema for a message type
 */
struct mp_edf_schema {
    const char *name;            ///< Schema name
    uint32_t version;           ///< Schema version
    mp_edf_field_t *fields;     ///< Array of field descriptors
    size_t field_count;         ///< Number of fields
    size_t struct_size;         ///< Size of the corresponding struct
};

/**
 * @brief EDF writer structure
 */
struct mp_edf_writer {
    uint8_t *buffer;            ///< Output buffer
    size_t position;           ///< Current position
    size_t capacity;           ///< Buffer capacity
    bool owns_buffer;          ///< Whether writer owns buffer
    
    // For dynamic allocation
    void *(*realloc_fn)(void *, size_t); ///< Reallocation function
    void *alloc_data;          ///< User data for realloc
};

/**
 * @brief EDF reader structure
 */
struct mp_edf_reader {
    const uint8_t *buffer;      ///< Input buffer
    size_t position;           ///< Current position
    size_t length;            ///< Buffer length
    bool owns_buffer;          ///< Whether reader owns buffer
};

/**
 * @brief EDF value structure (for dynamic reading)
 */
typedef struct {
    mp_edf_type_t type;         ///< Value type
    union {
        bool bool_value;
        int8_t int8_value;
        int16_t int16_value;
        int32_t int32_value;
        int64_t int64_value;
        uint8_t uint8_value;
        uint16_t uint16_value;
        uint32_t uint32_value;
        uint64_t uint64_value;
        float float_value;
        double double_value;
        struct {
            const char *value;     ///< String value
            size_t length;         ///< String length
        } string;
        struct {
            const uint8_t *value;  ///< Bytes value
            size_t length;         ///< Bytes length
        } bytes;
    } data;
} mp_edf_value_t;

/**
 * @brief Initialize an EDF writer
 * 
 * @param writer The writer to initialize
 * @param buffer Output buffer
 * @param capacity Buffer capacity
 * @return 0 on success, -1 on error
 */
int mp_edf_writer_init(mp_edf_writer_t *writer, uint8_t *buffer, size_t capacity);

/**
 * @brief Initialize an EDF writer with dynamic allocation
 * 
 * @param writer The writer to initialize
 * @param initial_capacity Initial buffer capacity
 * @param realloc_fn Reallocation function
 * @param alloc_data User data for realloc
 * @return 0 on success, -1 on error
 */
int mp_edf_writer_init_dynamic(mp_edf_writer_t *writer, size_t initial_capacity,
                              void *(*realloc_fn)(void *, size_t), void *alloc_data);

/**
 * @brief Destroy an EDF writer
 * 
 * @param writer The writer to destroy
 */
void mp_edf_writer_destroy(mp_edf_writer_t *writer);

/**
 * @brief Reset an EDF writer
 * 
 * @param writer The writer to reset
 */
void mp_edf_writer_reset(mp_edf_writer_t *writer);

/**
 * @brief Get written data as memory view
 * 
 * @param writer The writer
 * @return Memory view of written data
 */
mp_memory_view_t mp_edf_writer_get_data(mp_edf_writer_t *writer);

/**
 * @brief Get written data length
 * 
 * @param writer The writer
 * @return Length of written data
 */
size_t mp_edf_writer_get_length(mp_edf_writer_t *writer);

/**
 * @brief Initialize an EDF reader
 * 
 * @param reader The reader to initialize
 * @param buffer Input buffer
 * @param length Buffer length
 * @return 0 on success, -1 on error
 */
int mp_edf_reader_init(mp_edf_reader_t *reader, const uint8_t *buffer, size_t length);

/**
 * @brief Initialize an EDF reader from a memory view
 * 
 * @param reader The reader to initialize
 * @param view Memory view containing EDF data
 * @return 0 on success, -1 on error
 */
int mp_edf_reader_init_from_view(mp_edf_reader_t *reader, mp_memory_view_t *view);

/**
 * @brief Destroy an EDF reader
 * 
 * @param reader The reader to destroy
 */
void mp_edf_reader_destroy(mp_edf_reader_t *reader);

/**
 * @brief Get remaining bytes in reader
 * 
 * @param reader The reader
 * @return Number of remaining bytes
 */
size_t mp_edf_reader_remaining(mp_edf_reader_t *reader);

/**
 * @brief Get current position in reader
 * 
 * @param reader The reader
 * @return Current position
 */
size_t mp_edf_reader_position(mp_edf_reader_t *reader);

/**
 * @brief Set position in reader
 * 
 * @param reader The reader
 * @param position New position
 * @return 0 on success, -1 on error
 */
int mp_edf_reader_set_position(mp_edf_reader_t *reader, size_t position);

/**
 * @brief Write a type tag to the writer
 * 
 * @param writer The writer
 * @param type The type to write
 * @return 0 on success, -1 on error
 */
int mp_edf_write_type(mp_edf_writer_t *writer, mp_edf_type_t type);

/**
 * @brief Read a type tag from the reader
 * 
 * @param reader The reader
 * @param type Output variable for type
 * @return 0 on success, -1 on error
 */
int mp_edf_read_type(mp_edf_reader_t *reader, mp_edf_type_t *type);

/**
 * @brief Write a field header to the writer
 * 
 * @param writer The writer
 * @param field_id Field ID
 * @param type Field type
 * @return 0 on success, -1 on error
 */
int mp_edf_write_field_header(mp_edf_writer_t *writer, uint32_t field_id, mp_edf_type_t type);

/**
 * @brief Read a field header from the reader
 * 
 * @param reader The reader
 * @param field_id Output variable for field ID
 * @param type Output variable for field type
 * @return 0 on success, -1 on error
 */
int mp_edf_read_field_header(mp_edf_reader_t *reader, uint32_t *field_id, mp_edf_type_t *type);

/**
 * @brief Write a boolean value
 * 
 * @param writer The writer
 * @param value Boolean value
 * @return 0 on success, -1 on error
 */
int mp_edf_write_bool(mp_edf_writer_t *writer, bool value);

/**
 * @brief Read a boolean value
 * 
 * @param reader The reader
 * @param value Output variable for boolean
 * @return 0 on success, -1 on error
 */
int mp_edf_read_bool(mp_edf_reader_t *reader, bool *value);

/**
 * @brief Write an int8 value
 * 
 * @param writer The writer
 * @param value Int8 value
 * @return 0 on success, -1 on error
 */
int mp_edf_write_int8(mp_edf_writer_t *writer, int8_t value);

/**
 * @brief Read an int8 value
 * 
 * @param reader The reader
 * @param value Output variable for int8
 * @return 0 on success, -1 on error
 */
int mp_edf_read_int8(mp_edf_reader_t *reader, int8_t *value);

/**
 * @brief Write an int16 value
 * 
 * @param writer The writer
 * @param value Int16 value
 * @return 0 on success, -1 on error
 */
int mp_edf_write_int16(mp_edf_writer_t *writer, int16_t value);

/**
 * @brief Read an int16 value
 * 
 * @param reader The reader
 * @param value Output variable for int16
 * @return 0 on success, -1 on error
 */
int mp_edf_read_int16(mp_edf_reader_t *reader, int16_t *value);

/**
 * @brief Write an int32 value
 * 
 * @param writer The writer
 * @param value Int32 value
 * @return 0 on success, -1 on error
 */
int mp_edf_write_int32(mp_edf_writer_t *writer, int32_t value);

/**
 * @brief Read an int32 value
 * 
 * @param reader The reader
 * @param value Output variable for int32
 * @return 0 on success, -1 on error
 */
int mp_edf_read_int32(mp_edf_reader_t *reader, int32_t *value);

/**
 * @brief Write an int64 value
 * 
 * @param writer The writer
 * @param value Int64 value
 * @return 0 on success, -1 on error
 */
int mp_edf_write_int64(mp_edf_writer_t *writer, int64_t value);

/**
 * @brief Read an int64 value
 * 
 * @param reader The reader
 * @param value Output variable for int64
 * @return 0 on success, -1 on error
 */
int mp_edf_read_int64(mp_edf_reader_t *reader, int64_t *value);

/**
 * @brief Write a uint8 value
 * 
 * @param writer The writer
 * @param value Uint8 value
 * @return 0 on success, -1 on error
 */
int mp_edf_write_uint8(mp_edf_writer_t *writer, uint8_t value);

/**
 * @brief Read a uint8 value
 * 
 * @param reader The reader
 * @param value Output variable for uint8
 * @return 0 on success, -1 on error
 */
int mp_edf_read_uint8(mp_edf_reader_t *reader, uint8_t *value);

/**
 * @brief Write a uint16 value
 * 
 * @param writer The writer
 * @param value Uint16 value
 * @return 0 on success, -1 on error
 */
int mp_edf_write_uint16(mp_edf_writer_t *writer, uint16_t value);

/**
 * @brief Read a uint16 value
 * 
 * @param reader The reader
 * @param value Output variable for uint16
 * @return 0 on success, -1 on error
 */
int mp_edf_read_uint16(mp_edf_reader_t *reader, uint16_t *value);

/**
 * @brief Write a uint32 value
 * 
 * @param writer The writer
 * @param value Uint32 value
 * @return 0 on success, -1 on error
 */
int mp_edf_write_uint32(mp_edf_writer_t *writer, uint32_t value);

/**
 * @brief Read a uint32 value
 * 
 * @param reader The reader
 * @param value Output variable for uint32
 * @return 0 on success, -1 on error
 */
int mp_edf_read_uint32(mp_edf_reader_t *reader, uint32_t *value);

/**
 * @brief Write a uint64 value
 * 
 * @param writer The writer
 * @param value Uint64 value
 * @return 0 on success, -1 on error
 */
int mp_edf_write_uint64(mp_edf_writer_t *writer, uint64_t value);

/**
 * @brief Read a uint64 value
 * 
 * @param reader The reader
 * @param value Output variable for uint64
 * @return 0 on success, -1 on error
 */
int mp_edf_read_uint64(mp_edf_reader_t *reader, uint64_t *value);

/**
 * @brief Write a float value
 * 
 * @param writer The writer
 * @param value Float value
 * @return 0 on success, -1 on error
 */
int mp_edf_write_float(mp_edf_writer_t *writer, float value);

/**
 * @brief Read a float value
 * 
 * @param reader The reader
 * @param value Output variable for float
 * @return 0 on success, -1 on error
 */
int mp_edf_read_float(mp_edf_reader_t *reader, float *value);

/**
 * @brief Write a double value
 * 
 * @param writer The writer
 * @param value Double value
 * @return 0 on success, -1 on error
 */
int mp_edf_write_double(mp_edf_writer_t *writer, double value);

/**
 * @brief Read a double value
 * 
 * @param reader The reader
 * @param value Output variable for double
 * @return 0 on success, -1 on error
 */
int mp_edf_read_double(mp_edf_reader_t *reader, double *value);

/**
 * @brief Write a string value
 * 
 * @param writer The writer
 * @param str String to write (null-terminated)
 * @return 0 on success, -1 on error
 */
int mp_edf_write_string(mp_edf_writer_t *writer, const char *str);

/**
 * @brief Write a string with length
 * 
 * @param writer The writer
 * @param str String to write
 * @param length String length
 * @return 0 on success, -1 on error
 */
int mp_edf_write_string_with_length(mp_edf_writer_t *writer, const char *str, size_t length);

/**
 * @brief Read a string value
 * 
 * @param reader The reader
 * @param str Output buffer for string (null-terminated)
 * @param length Output variable for string length
 * @return 0 on success, -1 on error
 */
int mp_edf_read_string(mp_edf_reader_t *reader, char **str, size_t *length);

/**
 * @brief Write bytes value
 * 
 * @param writer The writer
 * @param data Bytes to write
 * @param length Length of bytes
 * @return 0 on success, -1 on error
 */
int mp_edf_write_bytes(mp_edf_writer_t *writer, const uint8_t *data, size_t length);

/**
 * @brief Read bytes value
 * 
 * @param reader The reader
 * @param data Output buffer for bytes
 * @param length Output variable for bytes length
 * @return 0 on success, -1 on error
 */
int mp_edf_read_bytes(mp_edf_reader_t *reader, uint8_t **data, size_t *length);

/**
 * @brief Write a length prefix
 * 
 * @param writer The writer
 * @param length Length to write
 * @return 0 on success, -1 on error
 */
int mp_edf_write_length(mp_edf_writer_t *writer, size_t length);

/**
 * @brief Read a length prefix
 * 
 * @param reader The reader
 * @param length Output variable for length
 * @return 0 on success, -1 on error
 */
int mp_edf_read_length(mp_edf_reader_t *reader, size_t *length);

/**
 * @brief Write a null value
 * 
 * @param writer The writer
 * @return 0 on success, -1 on error
 */
int mp_edf_write_null(mp_edf_writer_t *writer);

/**
 * @brief Read a null value
 * 
 * @param reader The reader
 * @return 0 on success, -1 on error
 */
int mp_edf_read_null(mp_edf_reader_t *reader);

/**
 * @brief Write an array header
 * 
 * @param writer The writer
 * @param element_type Element type
 * @param count Number of elements
 * @return 0 on success, -1 on error
 */
int mp_edf_write_array_header(mp_edf_writer_t *writer, mp_edf_type_t element_type, size_t count);

/**
 * @brief Read an array header
 * 
 * @param reader The reader
 * @param element_type Output variable for element type
 * @param count Output variable for element count
 * @return 0 on success, -1 on error
 */
int mp_edf_read_array_header(mp_edf_reader_t *reader, mp_edf_type_t *element_type, size_t *count);

/**
 * @brief Write an object header
 * 
 * @param writer The writer
 * @param schema_id Schema ID
 * @return 0 on success, -1 on error
 */
int mp_edf_write_object_header(mp_edf_writer_t *writer, uint32_t schema_id);

/**
 * @brief Read an object header
 * 
 * @param reader The reader
 * @param schema_id Output variable for schema ID
 * @return 0 on success, -1 on error
 */
int mp_edf_read_object_header(mp_edf_reader_t *reader, uint32_t *schema_id);

/**
 * @brief Write an object end marker
 * 
 * @param writer The writer
 * @return 0 on success, -1 on error
 */
int mp_edf_write_object_end(mp_edf_writer_t *writer);

/**
 * @brief Read an object end marker
 * 
 * @param reader The reader
 * @return 0 on success, -1 on error
 */
int mp_edf_read_object_end(mp_edf_reader_t *reader);

/**
 * @brief Skip a value of the given type
 * 
 * @param reader The reader
 * @param type Type to skip
 * @return 0 on success, -1 on error
 */
int mp_edf_skip_value(mp_edf_reader_t *reader, mp_edf_type_t type);

/**
 * @brief Skip a field
 * 
 * @param reader The reader
 * @return 0 on success, -1 on error
 */
int mp_edf_skip_field(mp_edf_reader_t *reader);

/**
 * @brief Encode a message using a schema
 * 
 * @param writer The writer
 * @param schema The schema
 * @param data Data to encode
 * @return 0 on success, -1 on error
 */
int mp_edf_encode_message(mp_edf_writer_t *writer, mp_edf_schema_t *schema, const void *data);

/**
 * @brief Decode a message using a schema
 * 
 * @param reader The reader
 * @param schema The schema
 * @param data Output buffer for decoded data
 * @return 0 on success, -1 on error
 */
int mp_edf_decode_message(mp_edf_reader_t *reader, mp_edf_schema_t *schema, void *data);

/**
 * @brief Encode a message to a buffer
 * 
 * @param buffer Output buffer
 * @param capacity Buffer capacity
 * @param schema The schema
 * @param data Data to encode
 * @return Number of bytes written, or -1 on error
 */
intptr_t mp_edf_encode_to_buffer(uint8_t *buffer, size_t capacity, mp_edf_schema_t *schema, const void *data);

/**
 * @brief Decode a message from a buffer
 * 
 * @param buffer Input buffer
 * @param length Buffer length
 * @param schema The schema
 * @param data Output buffer for decoded data
 * @return Number of bytes read, or -1 on error
 */
intptr_t mp_edf_decode_from_buffer(const uint8_t *buffer, size_t length, mp_edf_schema_t *schema, void *data);

/**
 * @brief Register a schema
 * 
 * @param schema The schema to register
 * @return 0 on success, -1 on error
 */
int mp_edf_register_schema(mp_edf_schema_t *schema);

/**
 * @brief Get a registered schema by ID
 * 
 * @param schema_id Schema ID
 * @return The schema, or NULL if not found
 */
mp_edf_schema_t *mp_edf_get_schema(uint32_t schema_id);

/**
 * @brief Get a registered schema by name
 * 
 * @param name Schema name
 * @return The schema, or NULL if not found
 */
mp_edf_schema_t *mp_edf_get_schema_by_name(const char *name);

/**
 * @brief Helper macro to define an EDF schema
 */
#define MP_EDF_SCHEMA(name, version) \
    static mp_edf_schema_t name##_schema = { \
        .name = #name, \
        .version = version, \
        .fields = NULL, \
        .field_count = 0, \
        .struct_size = sizeof(name) \
    }

/**
 * @brief Helper macro to add a field to a schema
 */
#define MP_EDF_FIELD(schema, type, name, id, required) \
    do { \
        static mp_edf_field_t field = { \
            .name = #name, \
            .id = id, \
            .type = type, \
            .required = required, \
            .repeated = false, \
            .type_name = NULL, \
            .offset = offsetof(schema, name), \
            .size = sizeof(((schema *)0)->name) \
        }; \
        /* Add to schema */ \
    } while (0)

/**
 * @brief Helper macro to define a simple EDF message
 */
#define MP_EDF_MESSAGE(name, version, ...) \
    typedef struct name name; \
    struct name { __VA_ARGS__ }; \
    MP_EDF_SCHEMA(name, version)

#endif // MICROPOSIX_EDF_H
