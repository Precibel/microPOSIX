#include "microposix/serialization/edf.h"
#include <string.h>
#include <stdlib.h>

// Default allocator
static void *default_realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}

// Schema registry
#define MAX_REGISTERED_SCHEMAS 64
static mp_edf_schema_t *registered_schemas[MAX_REGISTERED_SCHEMAS] = {0};
static size_t registered_schema_count = 0;

// Helper to ensure capacity
static void ensure_capacity(mp_edf_writer_t *writer, size_t needed) {
    if (!writer) return;
    
    if (writer->position + needed <= writer->capacity) {
        return;
    }
    
    // Need to reallocate
    size_t new_capacity = writer->capacity == 0 ? 256 : writer->capacity * 2;
    while (writer->position + needed > new_capacity) {
        new_capacity *= 2;
    }
    
    if (writer->realloc_fn) {
        uint8_t *new_buffer = (uint8_t *)writer->realloc_fn(writer->buffer, new_capacity);
        if (new_buffer) {
            writer->buffer = new_buffer;
            writer->capacity = new_capacity;
            writer->owns_buffer = true;
        }
    }
}

// Initialize writer
int mp_edf_writer_init(mp_edf_writer_t *writer, uint8_t *buffer, size_t capacity) {
    if (!writer) return -1;
    
    writer->buffer = buffer;
    writer->position = 0;
    writer->capacity = capacity;
    writer->owns_buffer = false;
    writer->realloc_fn = NULL;
    writer->alloc_data = NULL;
    
    return 0;
}

int mp_edf_writer_init_dynamic(mp_edf_writer_t *writer, size_t initial_capacity,
                              void *(*realloc_fn)(void *, size_t), void *alloc_data) {
    if (!writer) return -1;
    
    writer->buffer = NULL;
    writer->position = 0;
    writer->capacity = 0;
    writer->owns_buffer = true;
    writer->realloc_fn = realloc_fn ? realloc_fn : default_realloc;
    writer->alloc_data = alloc_data;
    
    if (initial_capacity > 0) {
        writer->buffer = (uint8_t *)writer->realloc_fn(NULL, initial_capacity);
        if (!writer->buffer) {
            return -1;
        }
        writer->capacity = initial_capacity;
    }
    
    return 0;
}

void mp_edf_writer_destroy(mp_edf_writer_t *writer) {
    if (!writer) return;
    
    if (writer->owns_buffer && writer->buffer && writer->realloc_fn) {
        writer->realloc_fn(writer->buffer, 0);
    }
    
    writer->buffer = NULL;
    writer->position = 0;
    writer->capacity = 0;
    writer->owns_buffer = false;
}

void mp_edf_writer_reset(mp_edf_writer_t *writer) {
    if (!writer) return;
    writer->position = 0;
}

mp_memory_view_t mp_edf_writer_get_data(mp_edf_writer_t *writer) {
    mp_memory_view_t view;
    if (!writer || !writer->buffer) {
        view.data = NULL;
        view.length = 0;
        view.capacity = 0;
        view.offset = 0;
        view.owns_data = false;
        view.free_fn = NULL;
        return view;
    }
    
    view.data = writer->buffer;
    view.length = writer->position;
    view.capacity = writer->capacity;
    view.offset = 0;
    view.owns_data = false;
    view.free_fn = NULL;
    return view;
}

size_t mp_edf_writer_get_length(mp_edf_writer_t *writer) {
    if (!writer) return 0;
    return writer->position;
}

// Initialize reader
int mp_edf_reader_init(mp_edf_reader_t *reader, const uint8_t *buffer, size_t length) {
    if (!reader) return -1;
    
    reader->buffer = buffer;
    reader->position = 0;
    reader->length = length;
    reader->owns_buffer = false;
    
    return 0;
}

int mp_edf_reader_init_from_view(mp_edf_reader_t *reader, mp_memory_view_t *view) {
    if (!reader || !view || !view->data) return -1;
    return mp_edf_reader_init(reader, view->data, view->length);
}

void mp_edf_reader_destroy(mp_edf_reader_t *reader) {
    if (!reader) return;
    
    if (reader->owns_buffer && reader->buffer) {
        free((void *)reader->buffer);
    }
    
    reader->buffer = NULL;
    reader->position = 0;
    reader->length = 0;
    reader->owns_buffer = false;
}

size_t mp_edf_reader_remaining(mp_edf_reader_t *reader) {
    if (!reader) return 0;
    return reader->length - reader->position;
}

size_t mp_edf_reader_position(mp_edf_reader_t *reader) {
    if (!reader) return 0;
    return reader->position;
}

int mp_edf_reader_set_position(mp_edf_reader_t *reader, size_t position) {
    if (!reader || position > reader->length) return -1;
    reader->position = position;
    return 0;
}

// Type encoding/decoding
int mp_edf_write_type(mp_edf_writer_t *writer, mp_edf_type_t type) {
    if (!writer) return -1;
    ensure_capacity(writer, 1);
    writer->buffer[writer->position++] = (uint8_t)type;
    return 0;
}

int mp_edf_read_type(mp_edf_reader_t *reader, mp_edf_type_t *type) {
    if (!reader || !type) return -1;
    if (reader->position >= reader->length) return -1;
    *type = (mp_edf_type_t)reader->buffer[reader->position++];
    return 0;
}

// Field header encoding/decoding
int mp_edf_write_field_header(mp_edf_writer_t *writer, uint32_t field_id, mp_edf_type_t type) {
    if (!writer) return -1;
    
    // Write field ID (varint)
    while (field_id >= 0x80) {
        ensure_capacity(writer, 1);
        writer->buffer[writer->position++] = (uint8_t)(field_id | 0x80);
        field_id >>= 7;
    }
    ensure_capacity(writer, 1);
    writer->buffer[writer->position++] = (uint8_t)field_id;
    
    // Write type
    return mp_edf_write_type(writer, type);
}

int mp_edf_read_field_header(mp_edf_reader_t *reader, uint32_t *field_id, mp_edf_type_t *type) {
    if (!reader || !field_id || !type) return -1;
    
    // Read field ID (varint)
    uint32_t id = 0;
    int shift = 0;
    
    while (1) {
        if (reader->position >= reader->length) return -1;
        
        uint8_t byte = reader->buffer[reader->position++];
        id |= (uint32_t)(byte & 0x7F) << shift;
        
        if ((byte & 0x80) == 0) break;
        
        shift += 7;
        if (shift >= 32) return -1; // Overflow
    }
    
    *field_id = id;
    
    // Read type
    return mp_edf_read_type(reader, type);
}

// Boolean encoding/decoding
int mp_edf_write_bool(mp_edf_writer_t *writer, bool value) {
    if (!writer) return -1;
    ensure_capacity(writer, 1);
    writer->buffer[writer->position++] = value ? 1 : 0;
    return 0;
}

int mp_edf_read_bool(mp_edf_reader_t *reader, bool *value) {
    if (!reader || !value) return -1;
    if (reader->position >= reader->length) return -1;
    *value = reader->buffer[reader->position++] != 0;
    return 0;
}

// Int8 encoding/decoding
int mp_edf_write_int8(mp_edf_writer_t *writer, int8_t value) {
    if (!writer) return -1;
    ensure_capacity(writer, 1);
    writer->buffer[writer->position++] = (uint8_t)value;
    return 0;
}

int mp_edf_read_int8(mp_edf_reader_t *reader, int8_t *value) {
    if (!reader || !value) return -1;
    if (reader->position >= reader->length) return -1;
    *value = (int8_t)reader->buffer[reader->position++];
    return 0;
}

// Int16 encoding/decoding
int mp_edf_write_int16(mp_edf_writer_t *writer, int16_t value) {
    if (!writer) return -1;
    ensure_capacity(writer, 2);
    writer->buffer[writer->position++] = (uint8_t)(value & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 8) & 0xFF);
    return 0;
}

int mp_edf_read_int16(mp_edf_reader_t *reader, int16_t *value) {
    if (!reader || !value) return -1;
    if (reader->position + 2 > reader->length) return -1;
    *value = (int16_t)(reader->buffer[reader->position] | (reader->buffer[reader->position + 1] << 8));
    reader->position += 2;
    return 0;
}

// Int32 encoding/decoding
int mp_edf_write_int32(mp_edf_writer_t *writer, int32_t value) {
    if (!writer) return -1;
    ensure_capacity(writer, 4);
    writer->buffer[writer->position++] = (uint8_t)(value & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 8) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 16) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 24) & 0xFF);
    return 0;
}

int mp_edf_read_int32(mp_edf_reader_t *reader, int32_t *value) {
    if (!reader || !value) return -1;
    if (reader->position + 4 > reader->length) return -1;
    *value = (int32_t)(reader->buffer[reader->position] | 
                      (reader->buffer[reader->position + 1] << 8) | 
                      (reader->buffer[reader->position + 2] << 16) | 
                      (reader->buffer[reader->position + 3] << 24));
    reader->position += 4;
    return 0;
}

// Int64 encoding/decoding
int mp_edf_write_int64(mp_edf_writer_t *writer, int64_t value) {
    if (!writer) return -1;
    ensure_capacity(writer, 8);
    writer->buffer[writer->position++] = (uint8_t)(value & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 8) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 16) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 24) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 32) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 40) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 48) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 56) & 0xFF);
    return 0;
}

int mp_edf_read_int64(mp_edf_reader_t *reader, int64_t *value) {
    if (!reader || !value) return -1;
    if (reader->position + 8 > reader->length) return -1;
    *value = (int64_t)(reader->buffer[reader->position] | 
                      (reader->buffer[reader->position + 1] << 8) | 
                      (reader->buffer[reader->position + 2] << 16) | 
                      (reader->buffer[reader->position + 3] << 24) |
                      (reader->buffer[reader->position + 4] << 32) | 
                      (reader->buffer[reader->position + 5] << 40) | 
                      (reader->buffer[reader->position + 6] << 48) | 
                      (reader->buffer[reader->position + 7] << 56));
    reader->position += 8;
    return 0;
}

// Uint8 encoding/decoding
int mp_edf_write_uint8(mp_edf_writer_t *writer, uint8_t value) {
    if (!writer) return -1;
    ensure_capacity(writer, 1);
    writer->buffer[writer->position++] = value;
    return 0;
}

int mp_edf_read_uint8(mp_edf_reader_t *reader, uint8_t *value) {
    if (!reader || !value) return -1;
    if (reader->position >= reader->length) return -1;
    *value = reader->buffer[reader->position++];
    return 0;
}

// Uint16 encoding/decoding
int mp_edf_write_uint16(mp_edf_writer_t *writer, uint16_t value) {
    if (!writer) return -1;
    ensure_capacity(writer, 2);
    writer->buffer[writer->position++] = (uint8_t)(value & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 8) & 0xFF);
    return 0;
}

int mp_edf_read_uint16(mp_edf_reader_t *reader, uint16_t *value) {
    if (!reader || !value) return -1;
    if (reader->position + 2 > reader->length) return -1;
    *value = (uint16_t)(reader->buffer[reader->position] | (reader->buffer[reader->position + 1] << 8));
    reader->position += 2;
    return 0;
}

// Uint32 encoding/decoding
int mp_edf_write_uint32(mp_edf_writer_t *writer, uint32_t value) {
    if (!writer) return -1;
    ensure_capacity(writer, 4);
    writer->buffer[writer->position++] = (uint8_t)(value & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 8) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 16) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 24) & 0xFF);
    return 0;
}

int mp_edf_read_uint32(mp_edf_reader_t *reader, uint32_t *value) {
    if (!reader || !value) return -1;
    if (reader->position + 4 > reader->length) return -1;
    *value = (uint32_t)(reader->buffer[reader->position] | 
                      (reader->buffer[reader->position + 1] << 8) | 
                      (reader->buffer[reader->position + 2] << 16) | 
                      (reader->buffer[reader->position + 3] << 24));
    reader->position += 4;
    return 0;
}

// Uint64 encoding/decoding
int mp_edf_write_uint64(mp_edf_writer_t *writer, uint64_t value) {
    if (!writer) return -1;
    ensure_capacity(writer, 8);
    writer->buffer[writer->position++] = (uint8_t)(value & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 8) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 16) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 24) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 32) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 40) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 48) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 56) & 0xFF);
    return 0;
}

int mp_edf_read_uint64(mp_edf_reader_t *reader, uint64_t *value) {
    if (!reader || !value) return -1;
    if (reader->position + 8 > reader->length) return -1;
    *value = (uint64_t)(reader->buffer[reader->position] | 
                      (reader->buffer[reader->position + 1] << 8) | 
                      (reader->buffer[reader->position + 2] << 16) | 
                      (reader->buffer[reader->position + 3] << 24) |
                      (reader->buffer[reader->position + 4] << 32) | 
                      (reader->buffer[reader->position + 5] << 40) | 
                      (reader->buffer[reader->position + 6] << 48) | 
                      (reader->buffer[reader->position + 7] << 56));
    reader->position += 8;
    return 0;
}

// Float encoding/decoding
int mp_edf_write_float(mp_edf_writer_t *writer, float value) {
    if (!writer) return -1;
    
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return mp_edf_write_uint32(writer, bits);
}

int mp_edf_read_float(mp_edf_reader_t *reader, float *value) {
    if (!reader || !value) return -1;
    
    uint32_t bits;
    if (mp_edf_read_uint32(reader, &bits) != 0) {
        return -1;
    }
    
    memcpy(value, &bits, sizeof(*value));
    return 0;
}

// Double encoding/decoding
int mp_edf_write_double(mp_edf_writer_t *writer, double value) {
    if (!writer) return -1;
    
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return mp_edf_write_uint64(writer, bits);
}

int mp_edf_read_double(mp_edf_reader_t *reader, double *value) {
    if (!reader || !value) return -1;
    
    uint64_t bits;
    if (mp_edf_read_uint64(reader, &bits) != 0) {
        return -1;
    }
    
    memcpy(value, &bits, sizeof(*value));
    return 0;
}

// String encoding/decoding
int mp_edf_write_string(mp_edf_writer_t *writer, const char *str) {
    if (!writer || !str) return -1;
    return mp_edf_write_string_with_length(writer, str, strlen(str));
}

int mp_edf_write_string_with_length(mp_edf_writer_t *writer, const char *str, size_t length) {
    if (!writer || (!str && length > 0)) return -1;
    
    // Write length
    if (mp_edf_write_length(writer, length) != 0) {
        return -1;
    }
    
    // Write string data
    ensure_capacity(writer, length);
    if (writer->position + length > writer->capacity) {
        return -1;
    }
    
    memcpy(writer->buffer + writer->position, str, length);
    writer->position += length;
    return 0;
}

int mp_edf_read_string(mp_edf_reader_t *reader, char **str, size_t *length) {
    if (!reader || !length) return -1;
    
    // Read length
    if (mp_edf_read_length(reader, length) != 0) {
        return -1;
    }
    
    if (reader->position + *length > reader->length) {
        return -1;
    }
    
    if (str) {
        *str = (char *)malloc(*length + 1);
        if (!*str) {
            return -1;
        }
        memcpy(*str, reader->buffer + reader->position, *length);
        (*str)[*length] = '\0';
    }
    
    reader->position += *length;
    return 0;
}

// Bytes encoding/decoding
int mp_edf_write_bytes(mp_edf_writer_t *writer, const uint8_t *data, size_t length) {
    if (!writer || (!data && length > 0)) return -1;
    
    // Write length
    if (mp_edf_write_length(writer, length) != 0) {
        return -1;
    }
    
    // Write bytes data
    ensure_capacity(writer, length);
    if (writer->position + length > writer->capacity) {
        return -1;
    }
    
    memcpy(writer->buffer + writer->position, data, length);
    writer->position += length;
    return 0;
}

int mp_edf_read_bytes(mp_edf_reader_t *reader, uint8_t **data, size_t *length) {
    if (!reader || !length) return -1;
    
    // Read length
    if (mp_edf_read_length(reader, length) != 0) {
        return -1;
    }
    
    if (reader->position + *length > reader->length) {
        return -1;
    }
    
    if (data) {
        *data = (uint8_t *)malloc(*length);
        if (!*data) {
            return -1;
        }
        memcpy(*data, reader->buffer + reader->position, *length);
    }
    
    reader->position += *length;
    return 0;
}

// Length encoding/decoding (varint)
int mp_edf_write_length(mp_edf_writer_t *writer, size_t length) {
    if (!writer) return -1;
    
    while (length >= 0x80) {
        ensure_capacity(writer, 1);
        writer->buffer[writer->position++] = (uint8_t)(length | 0x80);
        length >>= 7;
    }
    ensure_capacity(writer, 1);
    writer->buffer[writer->position++] = (uint8_t)length;
    return 0;
}

int mp_edf_read_length(mp_edf_reader_t *reader, size_t *length) {
    if (!reader || !length) return -1;
    
    size_t result = 0;
    int shift = 0;
    
    while (1) {
        if (reader->position >= reader->length) return -1;
        
        uint8_t byte = reader->buffer[reader->position++];
        result |= (size_t)(byte & 0x7F) << shift;
        
        if ((byte & 0x80) == 0) break;
        
        shift += 7;
        if (shift >= 64) return -1; // Overflow
    }
    
    *length = result;
    return 0;
}

// Null encoding/decoding
int mp_edf_write_null(mp_edf_writer_t *writer) {
    if (!writer) return -1;
    return mp_edf_write_type(writer, MP_EDF_NULL);
}

int mp_edf_read_null(mp_edf_reader_t *reader) {
    if (!reader) return -1;
    mp_edf_type_t type;
    if (mp_edf_read_type(reader, &type) != 0) {
        return -1;
    }
    if (type != MP_EDF_NULL) {
        return -1;
    }
    return 0;
}

// Array encoding/decoding
int mp_edf_write_array_header(mp_edf_writer_t *writer, mp_edf_type_t element_type, size_t count) {
    if (!writer) return -1;
    
    // Write array type
    if (mp_edf_write_type(writer, MP_EDF_ARRAY) != 0) {
        return -1;
    }
    
    // Write element type
    if (mp_edf_write_type(writer, element_type) != 0) {
        return -1;
    }
    
    // Write count
    return mp_edf_write_length(writer, count);
}

int mp_edf_read_array_header(mp_edf_reader_t *reader, mp_edf_type_t *element_type, size_t *count) {
    if (!reader || !element_type || !count) return -1;
    
    // Read array type
    mp_edf_type_t type;
    if (mp_edf_read_type(reader, &type) != 0) {
        return -1;
    }
    
    if (type != MP_EDF_ARRAY) {
        return -1;
    }
    
    // Read element type
    if (mp_edf_read_type(reader, element_type) != 0) {
        return -1;
    }
    
    // Read count
    return mp_edf_read_length(reader, count);
}

// Object encoding/decoding
int mp_edf_write_object_header(mp_edf_writer_t *writer, uint32_t schema_id) {
    if (!writer) return -1;
    
    // Write object type
    if (mp_edf_write_type(writer, MP_EDF_OBJECT) != 0) {
        return -1;
    }
    
    // Write schema ID
    return mp_edf_write_uint32(writer, schema_id);
}

int mp_edf_read_object_header(mp_edf_reader_t *reader, uint32_t *schema_id) {
    if (!reader || !schema_id) return -1;
    
    // Read object type
    mp_edf_type_t type;
    if (mp_edf_read_type(reader, &type) != 0) {
        return -1;
    }
    
    if (type != MP_EDF_OBJECT) {
        return -1;
    }
    
    // Read schema ID
    return mp_edf_read_uint32(reader, schema_id);
}

int mp_edf_write_object_end(mp_edf_writer_t *writer) {
    if (!writer) return -1;
    ensure_capacity(writer, 1);
    writer->buffer[writer->position++] = 0xFF; // End marker
    return 0;
}

int mp_edf_read_object_end(mp_edf_reader_t *reader) {
    if (!reader) return -1;
    if (reader->position >= reader->length) return -1;
    if (reader->buffer[reader->position++] != 0xFF) {
        return -1;
    }
    return 0;
}

// Skip functions
int mp_edf_skip_value(mp_edf_reader_t *reader, mp_edf_type_t type) {
    if (!reader) return -1;
    
    switch (type) {
        case MP_EDF_NULL:
            return 0;
        case MP_EDF_BOOL:
            reader->position += 1;
            return 0;
        case MP_EDF_INT8:
        case MP_EDF_UINT8:
            reader->position += 1;
            return 0;
        case MP_EDF_INT16:
        case MP_EDF_UINT16:
            reader->position += 2;
            return 0;
        case MP_EDF_INT32:
        case MP_EDF_UINT32:
        case MP_EDF_FLOAT:
        case MP_EDF_FIXED32:
        case MP_EDF_SFIXED32:
            reader->position += 4;
            return 0;
        case MP_EDF_INT64:
        case MP_EDF_UINT64:
        case MP_EDF_DOUBLE:
        case MP_EDF_FIXED64:
        case MP_EDF_SFIXED64:
            reader->position += 8;
            return 0;
        case MP_EDF_STRING:
        case MP_EDF_BYTES: {
            size_t length;
            if (mp_edf_read_length(reader, &length) != 0) {
                return -1;
            }
            reader->position += length;
            return 0;
        }
        case MP_EDF_ARRAY: {
            mp_edf_type_t element_type;
            size_t count;
            if (mp_edf_read_array_header(reader, &element_type, &count) != 0) {
                return -1;
            }
            for (size_t i = 0; i < count; i++) {
                if (mp_edf_skip_value(reader, element_type) != 0) {
                    return -1;
                }
            }
            return 0;
        }
        case MP_EDF_OBJECT: {
            uint32_t schema_id;
            if (mp_edf_read_object_header(reader, &schema_id) != 0) {
                return -1;
            }
            // Skip all fields until end marker
            while (reader->position < reader->length) {
                if (reader->buffer[reader->position] == 0xFF) {
                    reader->position++;
                    break;
                }
                // Skip field
                uint32_t field_id;
                mp_edf_type_t field_type;
                if (mp_edf_read_field_header(reader, &field_id, &field_type) != 0) {
                    return -1;
                }
                if (mp_edf_skip_value(reader, field_type) != 0) {
                    return -1;
                }
            }
            return 0;
        }
        default:
            return -1;
    }
}

int mp_edf_skip_field(mp_edf_reader_t *reader) {
    if (!reader) return -1;
    
    uint32_t field_id;
    mp_edf_type_t type;
    
    if (mp_edf_read_field_header(reader, &field_id, &type) != 0) {
        return -1;
    }
    
    return mp_edf_skip_value(reader, type);
}

// Schema registry
int mp_edf_register_schema(mp_edf_schema_t *schema) {
    if (!schema || registered_schema_count >= MAX_REGISTERED_SCHEMAS) {
        return -1;
    }
    
    // Check for duplicate
    for (size_t i = 0; i < registered_schema_count; i++) {
        if (registered_schemas[i] == schema) {
            return 0; // Already registered
        }
    }
    
    registered_schemas[registered_schema_count++] = schema;
    return 0;
}

mp_edf_schema_t *mp_edf_get_schema(uint32_t schema_id) {
    (void)schema_id;
    // TODO: Implement schema lookup by ID
    return NULL;
}

mp_edf_schema_t *mp_edf_get_schema_by_name(const char *name) {
    if (!name) return NULL;
    
    for (size_t i = 0; i < registered_schema_count; i++) {
        if (registered_schemas[i] && registered_schemas[i]->name && 
            strcmp(registered_schemas[i]->name, name) == 0) {
            return registered_schemas[i];
        }
    }
    
    return NULL;
}

// Message encoding/decoding
int mp_edf_encode_message(mp_edf_writer_t *writer, mp_edf_schema_t *schema, const void *data) {
    (void)writer;
    (void)schema;
    (void)data;
    // TODO: Implement message encoding
    return -1;
}

int mp_edf_decode_message(mp_edf_reader_t *reader, mp_edf_schema_t *schema, void *data) {
    (void)reader;
    (void)schema;
    (void)data;
    // TODO: Implement message decoding
    return -1;
}

intptr_t mp_edf_encode_to_buffer(uint8_t *buffer, size_t capacity, mp_edf_schema_t *schema, const void *data) {
    mp_edf_writer_t writer;
    if (mp_edf_writer_init(&writer, buffer, capacity) != 0) {
        return -1;
    }
    
    if (mp_edf_encode_message(&writer, schema, data) != 0) {
        return -1;
    }
    
    return (intptr_t)writer.position;
}

intptr_t mp_edf_decode_from_buffer(const uint8_t *buffer, size_t length, mp_edf_schema_t *schema, void *data) {
    mp_edf_reader_t reader;
    if (mp_edf_reader_init(&reader, buffer, length) != 0) {
        return -1;
    }
    
    if (mp_edf_decode_message(&reader, schema, data) != 0) {
        return -1;
    }
    
    return (intptr_t)reader.position;
}
