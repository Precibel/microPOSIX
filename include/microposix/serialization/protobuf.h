#ifndef MICROPOSIX_PROTOBUF_H
#define MICROPOSIX_PROTOBUF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "microposix/mm/memory_view.h"

/**
 * @file protobuf.h
 * @brief Protocol Buffers (protobuf) serialization for embedded systems
 * 
 * Lightweight protobuf implementation for memory-constrained environments.
 * Supports encoding and decoding of protobuf messages with minimal overhead.
 * 
 * This is a simplified implementation that supports the most common protobuf
 * types and encoding rules. For full protobuf support, consider using nanopb
 * or similar libraries.
 */

// Forward declarations
typedef struct mp_protobuf_writer mp_protobuf_writer_t;
typedef struct mp_protobuf_reader mp_protobuf_reader_t;
typedef struct mp_protobuf_field mp_protobuf_field_t;
typedef struct mp_protobuf_message mp_protobuf_message_t;

/**
 * @brief Protobuf wire types
 */
typedef enum {
    MP_PROTOBUF_VARINT = 0,      ///< Varint encoding
    MP_PROTOBUF_FIXED64 = 1,     ///< 64-bit fixed
    MP_PROTOBUF_LENGTH_DELIMITED = 2, ///< Length-delimited (string, bytes, message)
    MP_PROTOBUF_FIXED32 = 5,     ///< 32-bit fixed
} mp_protobuf_wire_type_t;

/**
 * @brief Protobuf field types
 */
typedef enum {
    MP_PROTOBUF_DOUBLE = 1,
    MP_PROTOBUF_FLOAT = 2,
    MP_PROTOBUF_INT32 = 3,
    MP_PROTOBUF_INT64 = 4,
    MP_PROTOBUF_UINT32 = 5,
    MP_PROTOBUF_UINT64 = 6,
    MP_PROTOBUF_SINT32 = 7,
    MP_PROTOBUF_SINT64 = 8,
    MP_PROTOBUF_TYPE_FIXED32 = 9,
    MP_PROTOBUF_TYPE_FIXED64 = 10,
    MP_PROTOBUF_SFIXED32 = 11,
    MP_PROTOBUF_SFIXED64 = 12,
    MP_PROTOBUF_BOOL = 13,
    MP_PROTOBUF_STRING = 14,
    MP_PROTOBUF_BYTES = 15,
    MP_PROTOBUF_MESSAGE = 16,
    MP_PROTOBUF_ENUM = 17
} mp_protobuf_field_type_t;

/**
 * @brief Protobuf field label
 */
typedef enum {
    MP_PROTOBUF_REQUIRED = 0,
    MP_PROTOBUF_OPTIONAL = 1,
    MP_PROTOBUF_REPEATED = 2
} mp_protobuf_field_label_t;

/**
 * @brief Protobuf field descriptor
 */
struct mp_protobuf_field {
    uint32_t number;             ///< Field number
    const char *name;            ///< Field name
    mp_protobuf_field_type_t type; ///< Field type
    mp_protobuf_field_label_t label; ///< Field label
    const char *message_type;   ///< Message type name (for message fields)
    const char *enum_type;      ///< Enum type name (for enum fields)
    void *default_value;        ///< Default value
    size_t default_value_size;  ///< Size of default value
};

/**
 * @brief Protobuf message descriptor
 */
typedef struct {
    const char *name;            ///< Message name
    mp_protobuf_field_t *fields; ///< Array of field descriptors
    size_t field_count;         ///< Number of fields
    size_t size;               ///< Size of message struct
} mp_protobuf_message_descriptor_t;

/**
 * @brief Protobuf writer structure
 */
struct mp_protobuf_writer {
    uint8_t *buffer;            ///< Output buffer
    size_t position;           ///< Current position in buffer
    size_t capacity;           ///< Total capacity of buffer
    bool owns_buffer;          ///< Whether writer owns the buffer
    
    // For dynamic allocation
    void *(*realloc_fn)(void *, size_t); ///< Reallocation function
    void *alloc_data;          ///< User data for realloc
};

/**
 * @brief Protobuf reader structure
 */
struct mp_protobuf_reader {
    const uint8_t *buffer;      ///< Input buffer
    size_t position;           ///< Current position in buffer
    size_t length;            ///< Total length of buffer
    bool owns_buffer;          ///< Whether reader owns the buffer
};

/**
 * @brief Initialize a protobuf writer
 * 
 * @param writer The writer to initialize
 * @param buffer Output buffer
 * @param capacity Buffer capacity
 * @return 0 on success, -1 on error
 */
int mp_protobuf_writer_init(mp_protobuf_writer_t *writer, uint8_t *buffer, size_t capacity);

/**
 * @brief Initialize a protobuf writer with dynamic allocation
 * 
 * @param writer The writer to initialize
 * @param initial_capacity Initial buffer capacity
 * @param realloc_fn Reallocation function
 * @param alloc_data User data for realloc
 * @return 0 on success, -1 on error
 */
int mp_protobuf_writer_init_dynamic(mp_protobuf_writer_t *writer, size_t initial_capacity,
                                    void *(*realloc_fn)(void *, size_t), void *alloc_data);

/**
 * @brief Destroy a protobuf writer
 * 
 * @param writer The writer to destroy
 */
void mp_protobuf_writer_destroy(mp_protobuf_writer_t *writer);

/**
 * @brief Reset a protobuf writer
 * 
 * @param writer The writer to reset
 */
void mp_protobuf_writer_reset(mp_protobuf_writer_t *writer);

/**
 * @brief Get the written data
 * 
 * @param writer The writer
 * @return Memory view of written data
 */
mp_memory_view_t mp_protobuf_writer_get_data(mp_protobuf_writer_t *writer);

/**
 * @brief Get the written data length
 * 
 * @param writer The writer
 * @return Length of written data
 */
size_t mp_protobuf_writer_get_length(mp_protobuf_writer_t *writer);

/**
 * @brief Initialize a protobuf reader
 * 
 * @param reader The reader to initialize
 * @param buffer Input buffer
 * @param length Buffer length
 * @return 0 on success, -1 on error
 */
int mp_protobuf_reader_init(mp_protobuf_reader_t *reader, const uint8_t *buffer, size_t length);

/**
 * @brief Initialize a protobuf reader from a memory view
 * 
 * @param reader The reader to initialize
 * @param view Memory view containing protobuf data
 * @return 0 on success, -1 on error
 */
int mp_protobuf_reader_init_from_view(mp_protobuf_reader_t *reader, mp_memory_view_t *view);

/**
 * @brief Destroy a protobuf reader
 * 
 * @param reader The reader to destroy
 */
void mp_protobuf_reader_destroy(mp_protobuf_reader_t *reader);

/**
 * @brief Get remaining bytes in reader
 * 
 * @param reader The reader
 * @return Number of remaining bytes
 */
size_t mp_protobuf_reader_remaining(mp_protobuf_reader_t *reader);

/**
 * @brief Get current position in reader
 * 
 * @param reader The reader
 * @return Current position
 */
size_t mp_protobuf_reader_position(mp_protobuf_reader_t *reader);

/**
 * @brief Set position in reader
 * 
 * @param reader The reader
 * @param position New position
 * @return 0 on success, -1 on error
 */
int mp_protobuf_reader_set_position(mp_protobuf_reader_t *reader, size_t position);

/**
 * @brief Write a varint to the writer
 * 
 * @param writer The writer
 * @param value The value to write
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_varint(mp_protobuf_writer_t *writer, uint64_t value);

/**
 * @brief Write a fixed32 to the writer
 * 
 * @param writer The writer
 * @param value The value to write
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_fixed32(mp_protobuf_writer_t *writer, uint32_t value);

/**
 * @brief Write a fixed64 to the writer
 * 
 * @param writer The writer
 * @param value The value to write
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_fixed64(mp_protobuf_writer_t *writer, uint64_t value);

/**
 * @brief Write a length-delimited value to the writer
 * 
 * @param writer The writer
 * @param data Data to write
 * @param length Length of data
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_length_delimited(mp_protobuf_writer_t *writer, const uint8_t *data, size_t length);

/**
 * @brief Write a string to the writer
 * 
 * @param writer The writer
 * @param str String to write (null-terminated)
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_string(mp_protobuf_writer_t *writer, const char *str);

/**
 * @brief Write a string with length to the writer
 * 
 * @param writer The writer
 * @param str String to write
 * @param length Length of string
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_string_with_length(mp_protobuf_writer_t *writer, const char *str, size_t length);

/**
 * @brief Write bytes to the writer
 * 
 * @param writer The writer
 * @param data Data to write
 * @param length Length of data
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_bytes(mp_protobuf_writer_t *writer, const uint8_t *data, size_t length);

/**
 * @brief Write a boolean to the writer
 * 
 * @param writer The writer
 * @param value Boolean value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_bool(mp_protobuf_writer_t *writer, bool value);

/**
 * @brief Write an enum to the writer
 * 
 * @param writer The writer
 * @param value Enum value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_enum(mp_protobuf_writer_t *writer, int32_t value);

/**
 * @brief Write a field header to the writer
 * 
 * @param writer The writer
 * @param field_number Field number
 * @param wire_type Wire type
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_field_header(mp_protobuf_writer_t *writer, uint32_t field_number, mp_protobuf_wire_type_t wire_type);

/**
 * @brief Write a double to the writer
 * 
 * @param writer The writer
 * @param value Double value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_double(mp_protobuf_writer_t *writer, double value);

/**
 * @brief Write a float to the writer
 * 
 * @param writer The writer
 * @param value Float value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_float(mp_protobuf_writer_t *writer, float value);

/**
 * @brief Write an int32 to the writer
 * 
 * @param writer The writer
 * @param value Int32 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_int32(mp_protobuf_writer_t *writer, int32_t value);

/**
 * @brief Write an int64 to the writer
 * 
 * @param writer The writer
 * @param value Int64 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_int64(mp_protobuf_writer_t *writer, int64_t value);

/**
 * @brief Write a uint32 to the writer
 * 
 * @param writer The writer
 * @param value Uint32 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_uint32(mp_protobuf_writer_t *writer, uint32_t value);

/**
 * @brief Write a uint64 to the writer
 * 
 * @param writer The writer
 * @param value Uint64 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_uint64(mp_protobuf_writer_t *writer, uint64_t value);

/**
 * @brief Write a sint32 to the writer (zig-zag encoded)
 * 
 * @param writer The writer
 * @param value Sint32 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_sint32(mp_protobuf_writer_t *writer, int32_t value);

/**
 * @brief Write a sint64 to the writer (zig-zag encoded)
 * 
 * @param writer The writer
 * @param value Sint64 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_sint64(mp_protobuf_writer_t *writer, int64_t value);

/**
 * @brief Write a fixed32 to the writer
 * 
 * @param writer The writer
 * @param value Fixed32 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_sfixed32(mp_protobuf_writer_t *writer, int32_t value);

/**
 * @brief Write a fixed64 to the writer
 * 
 * @param writer The writer
 * @param value Fixed64 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_write_sfixed64(mp_protobuf_writer_t *writer, int64_t value);

/**
 * @brief Read a varint from the reader
 * 
 * @param reader The reader
 * @param value Output variable for the value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_varint(mp_protobuf_reader_t *reader, uint64_t *value);

/**
 * @brief Read a fixed32 from the reader
 * 
 * @param reader The reader
 * @param value Output variable for the value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_fixed32(mp_protobuf_reader_t *reader, uint32_t *value);

/**
 * @brief Read a fixed64 from the reader
 * 
 * @param reader The reader
 * @param value Output variable for the value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_fixed64(mp_protobuf_reader_t *reader, uint64_t *value);

/**
 * @brief Read a length-delimited value from the reader
 * 
 * @param reader The reader
 * @param data Output buffer for data
 * @param length Output variable for length
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_length_delimited(mp_protobuf_reader_t *reader, uint8_t **data, size_t *length);

/**
 * @brief Read a string from the reader
 * 
 * @param reader The reader
 * @param str Output buffer for string (null-terminated)
 * @param length Output variable for string length
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_string(mp_protobuf_reader_t *reader, char **str, size_t *length);

/**
 * @brief Read bytes from the reader
 * 
 * @param reader The reader
 * @param data Output buffer for data
 * @param length Output variable for data length
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_bytes(mp_protobuf_reader_t *reader, uint8_t **data, size_t *length);

/**
 * @brief Read a boolean from the reader
 * 
 * @param reader The reader
 * @param value Output variable for boolean value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_bool(mp_protobuf_reader_t *reader, bool *value);

/**
 * @brief Read an enum from the reader
 * 
 * @param reader The reader
 * @param value Output variable for enum value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_enum(mp_protobuf_reader_t *reader, int32_t *value);

/**
 * @brief Read a field header from the reader
 * 
 * @param reader The reader
 * @param field_number Output variable for field number
 * @param wire_type Output variable for wire type
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_field_header(mp_protobuf_reader_t *reader, uint32_t *field_number, mp_protobuf_wire_type_t *wire_type);

/**
 * @brief Read a double from the reader
 * 
 * @param reader The reader
 * @param value Output variable for double value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_double(mp_protobuf_reader_t *reader, double *value);

/**
 * @brief Read a float from the reader
 * 
 * @param reader The reader
 * @param value Output variable for float value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_float(mp_protobuf_reader_t *reader, float *value);

/**
 * @brief Read an int32 from the reader
 * 
 * @param reader The reader
 * @param value Output variable for int32 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_int32(mp_protobuf_reader_t *reader, int32_t *value);

/**
 * @brief Read an int64 from the reader
 * 
 * @param reader The reader
 * @param value Output variable for int64 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_int64(mp_protobuf_reader_t *reader, int64_t *value);

/**
 * @brief Read a uint32 from the reader
 * 
 * @param reader The reader
 * @param value Output variable for uint32 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_uint32(mp_protobuf_reader_t *reader, uint32_t *value);

/**
 * @brief Read a uint64 from the reader
 * 
 * @param reader The reader
 * @param value Output variable for uint64 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_uint64(mp_protobuf_reader_t *reader, uint64_t *value);

/**
 * @brief Read a sint32 from the reader (zig-zag decoded)
 * 
 * @param reader The reader
 * @param value Output variable for sint32 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_sint32(mp_protobuf_reader_t *reader, int32_t *value);

/**
 * @brief Read a sint64 from the reader (zig-zag decoded)
 * 
 * @param reader The reader
 * @param value Output variable for sint64 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_sint64(mp_protobuf_reader_t *reader, int64_t *value);

/**
 * @brief Read a sfixed32 from the reader
 * 
 * @param reader The reader
 * @param value Output variable for sfixed32 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_sfixed32(mp_protobuf_reader_t *reader, int32_t *value);

/**
 * @brief Read a sfixed64 from the reader
 * 
 * @param reader The reader
 * @param value Output variable for sfixed64 value
 * @return 0 on success, -1 on error
 */
int mp_protobuf_read_sfixed64(mp_protobuf_reader_t *reader, int64_t *value);

/**
 * @brief Skip a field in the reader
 * 
 * @param reader The reader
 * @param wire_type Wire type of the field to skip
 * @return 0 on success, -1 on error
 */
int mp_protobuf_skip_field(mp_protobuf_reader_t *reader, mp_protobuf_wire_type_t wire_type);

/**
 * @brief Skip an unknown field in the reader
 * 
 * @param reader The reader
 * @return 0 on success, -1 on error
 */
int mp_protobuf_skip_unknown(mp_protobuf_reader_t *reader);

/**
 * @brief Encode a zig-zag encoded 32-bit integer
 * 
 * @param n The integer to encode
 * @return Encoded value
 */
int32_t mp_protobuf_zigzag32(int32_t n);

/**
 * @brief Decode a zig-zag encoded 32-bit integer
 * 
 * @param n The encoded value
 * @return Decoded value
 */
int32_t mp_protobuf_unzigzag32(uint32_t n);

/**
 * @brief Encode a zig-zag encoded 64-bit integer
 * 
 * @param n The integer to encode
 * @return Encoded value
 */
int64_t mp_protobuf_zigzag64(int64_t n);

/**
 * @brief Decode a zig-zag encoded 64-bit integer
 * 
 * @param n The encoded value
 * @return Decoded value
 */
int64_t mp_protobuf_unzigzag64(uint64_t n);

/**
 * @brief Calculate the size of a varint
 * 
 * @param value The value
 * @return Size in bytes
 */
size_t mp_protobuf_varint_size(uint64_t value);

/**
 * @brief Calculate the size of a field header
 * 
 * @param field_number Field number
 * @param wire_type Wire type
 * @return Size in bytes
 */
size_t mp_protobuf_field_header_size(uint32_t field_number, mp_protobuf_wire_type_t wire_type);

/**
 * @brief Calculate the size of a length-delimited field
 * 
 * @param field_number Field number
 * @param length Length of data
 * @return Size in bytes
 */
size_t mp_protobuf_length_delimited_size(uint32_t field_number, size_t length);

/**
 * @brief Calculate the total size of a message
 * 
 * @param descriptor Message descriptor
 * @param message Message data
 * @return Size in bytes, or -1 on error
 */
intptr_t mp_protobuf_message_size(mp_protobuf_message_descriptor_t *descriptor, const void *message);

/**
 * @brief Encode a message to a writer
 * 
 * @param writer The writer
 * @param descriptor Message descriptor
 * @param message Message data
 * @return 0 on success, -1 on error
 */
int mp_protobuf_encode_message(mp_protobuf_writer_t *writer, 
                               mp_protobuf_message_descriptor_t *descriptor, 
                               const void *message);

/**
 * @brief Decode a message from a reader
 * 
 * @param reader The reader
 * @param descriptor Message descriptor
 * @param message Output buffer for message
 * @return 0 on success, -1 on error
 */
int mp_protobuf_decode_message(mp_protobuf_reader_t *reader,
                               mp_protobuf_message_descriptor_t *descriptor,
                               void *message);

/**
 * @brief Encode a message to a buffer
 * 
 * @param buffer Output buffer
 * @param capacity Buffer capacity
 * @param descriptor Message descriptor
 * @param message Message data
 * @return Number of bytes written, or -1 on error
 */
intptr_t mp_protobuf_encode_to_buffer(uint8_t *buffer, size_t capacity,
                                     mp_protobuf_message_descriptor_t *descriptor,
                                     const void *message);

/**
 * @brief Decode a message from a buffer
 * 
 * @param buffer Input buffer
 * @param length Buffer length
 * @param descriptor Message descriptor
 * @param message Output buffer for message
 * @return Number of bytes read, or -1 on error
 */
intptr_t mp_protobuf_decode_from_buffer(const uint8_t *buffer, size_t length,
                                        mp_protobuf_message_descriptor_t *descriptor,
                                        void *message);

/**
 * @brief Helper macro to define a protobuf message
 */
#define MP_PROTOBUF_MESSAGE(name) \
    typedef struct name name; \
    extern mp_protobuf_message_descriptor_t name##_descriptor

/**
 * @brief Helper macro to define a protobuf field
 */
#define MP_PROTOBUF_FIELD(type, name, number, label) \
    type name

/**
 * @brief Helper macro to define a required field
 */
#define MP_PROTOBUF_REQUIRED(type, name, number) \
    MP_PROTOBUF_FIELD(type, name, number, MP_PROTOBUF_REQUIRED)

/**
 * @brief Helper macro to define an optional field
 */
#define MP_PROTOBUF_OPTIONAL(type, name, number) \
    MP_PROTOBUF_FIELD(type, name, number, MP_PROTOBUF_OPTIONAL)

/**
 * @brief Helper macro to define a repeated field
 */
#define MP_PROTOBUF_REPEATED(type, name, number) \
    MP_PROTOBUF_FIELD(type, name, number, MP_PROTOBUF_REPEATED)

/**
 * @brief Helper macro to get field offset
 */
#define MP_PROTOBUF_FIELD_OFFSET(struct_type, field) offsetof(struct_type, field)

/**
 * @brief Helper macro to get field size
 */
#define MP_PROTOBUF_FIELD_SIZE(struct_type, field) sizeof(((struct_type *)0)->field)

#endif // MICROPOSIX_PROTOBUF_H
