#include "microposix/serialization/protobuf.h"
#include <string.h>
#include <stdlib.h>

// Default allocator
static void *default_realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}

// Helper to ensure capacity
static void ensure_capacity(mp_protobuf_writer_t *writer, size_t needed) {
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
int mp_protobuf_writer_init(mp_protobuf_writer_t *writer, uint8_t *buffer, size_t capacity) {
    if (!writer) return -1;
    
    writer->buffer = buffer;
    writer->position = 0;
    writer->capacity = capacity;
    writer->owns_buffer = false;
    writer->realloc_fn = NULL;
    writer->alloc_data = NULL;
    
    return 0;
}

int mp_protobuf_writer_init_dynamic(mp_protobuf_writer_t *writer, size_t initial_capacity,
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

void mp_protobuf_writer_destroy(mp_protobuf_writer_t *writer) {
    if (!writer) return;
    
    if (writer->owns_buffer && writer->buffer && writer->realloc_fn) {
        writer->realloc_fn(writer->buffer, 0);
    }
    
    writer->buffer = NULL;
    writer->position = 0;
    writer->capacity = 0;
    writer->owns_buffer = false;
}

void mp_protobuf_writer_reset(mp_protobuf_writer_t *writer) {
    if (!writer) return;
    writer->position = 0;
}

mp_memory_view_t mp_protobuf_writer_get_data(mp_protobuf_writer_t *writer) {
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

size_t mp_protobuf_writer_get_length(mp_protobuf_writer_t *writer) {
    if (!writer) return 0;
    return writer->position;
}

// Initialize reader
int mp_protobuf_reader_init(mp_protobuf_reader_t *reader, const uint8_t *buffer, size_t length) {
    if (!reader) return -1;
    
    reader->buffer = buffer;
    reader->position = 0;
    reader->length = length;
    reader->owns_buffer = false;
    
    return 0;
}

int mp_protobuf_reader_init_from_view(mp_protobuf_reader_t *reader, mp_memory_view_t *view) {
    if (!reader || !view || !view->data) return -1;
    return mp_protobuf_reader_init(reader, view->data, view->length);
}

void mp_protobuf_reader_destroy(mp_protobuf_reader_t *reader) {
    if (!reader) return;
    
    if (reader->owns_buffer && reader->buffer) {
        free((void *)reader->buffer);
    }
    
    reader->buffer = NULL;
    reader->position = 0;
    reader->length = 0;
    reader->owns_buffer = false;
}

size_t mp_protobuf_reader_remaining(mp_protobuf_reader_t *reader) {
    if (!reader) return 0;
    return reader->length - reader->position;
}

size_t mp_protobuf_reader_position(mp_protobuf_reader_t *reader) {
    if (!reader) return 0;
    return reader->position;
}

int mp_protobuf_reader_set_position(mp_protobuf_reader_t *reader, size_t position) {
    if (!reader || position > reader->length) return -1;
    reader->position = position;
    return 0;
}

// Varint encoding/decoding
int mp_protobuf_write_varint(mp_protobuf_writer_t *writer, uint64_t value) {
    if (!writer) return -1;
    
    ensure_capacity(writer, 10); // Max varint size is 10 bytes
    
    while (value >= 0x80) {
        writer->buffer[writer->position++] = (uint8_t)(value | 0x80);
        value >>= 7;
    }
    writer->buffer[writer->position++] = (uint8_t)value;
    
    return 0;
}

int mp_protobuf_read_varint(mp_protobuf_reader_t *reader, uint64_t *value) {
    if (!reader || !value) return -1;
    
    uint64_t result = 0;
    int shift = 0;
    
    while (1) {
        if (reader->position >= reader->length) {
            return -1; // Unexpected end
        }
        
        uint8_t byte = reader->buffer[reader->position++];
        result |= (uint64_t)(byte & 0x7F) << shift;
        
        if ((byte & 0x80) == 0) {
            break;
        }
        
        shift += 7;
        if (shift >= 64) {
            return -1; // Overflow
        }
    }
    
    *value = result;
    return 0;
}

// Fixed32 encoding/decoding
int mp_protobuf_write_fixed32(mp_protobuf_writer_t *writer, uint32_t value) {
    if (!writer) return -1;
    ensure_capacity(writer, 4);
    
    writer->buffer[writer->position++] = (uint8_t)(value & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 8) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 16) & 0xFF);
    writer->buffer[writer->position++] = (uint8_t)((value >> 24) & 0xFF);
    
    return 0;
}

int mp_protobuf_read_fixed32(mp_protobuf_reader_t *reader, uint32_t *value) {
    if (!reader || !value) return -1;
    
    if (reader->position + 4 > reader->length) {
        return -1; // Not enough data
    }
    
    uint32_t result = 0;
    result |= (uint32_t)reader->buffer[reader->position++];
    result |= (uint32_t)reader->buffer[reader->position++] << 8;
    result |= (uint32_t)reader->buffer[reader->position++] << 16;
    result |= (uint32_t)reader->buffer[reader->position++] << 24;
    
    *value = result;
    return 0;
}

// Fixed64 encoding/decoding
int mp_protobuf_write_fixed64(mp_protobuf_writer_t *writer, uint64_t value) {
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

int mp_protobuf_read_fixed64(mp_protobuf_reader_t *reader, uint64_t *value) {
    if (!reader || !value) return -1;
    
    if (reader->position + 8 > reader->length) {
        return -1; // Not enough data
    }
    
    uint64_t result = 0;
    result |= (uint64_t)reader->buffer[reader->position++];
    result |= (uint64_t)reader->buffer[reader->position++] << 8;
    result |= (uint64_t)reader->buffer[reader->position++] << 16;
    result |= (uint64_t)reader->buffer[reader->position++] << 24;
    result |= (uint64_t)reader->buffer[reader->position++] << 32;
    result |= (uint64_t)reader->buffer[reader->position++] << 40;
    result |= (uint64_t)reader->buffer[reader->position++] << 48;
    result |= (uint64_t)reader->buffer[reader->position++] << 56;
    
    *value = result;
    return 0;
}

// Length-delimited encoding/decoding
int mp_protobuf_write_length_delimited(mp_protobuf_writer_t *writer, const uint8_t *data, size_t length) {
    if (!writer || (!data && length > 0)) return -1;
    
    // Write length as varint
    if (mp_protobuf_write_varint(writer, length) != 0) {
        return -1;
    }
    
    // Write data
    ensure_capacity(writer, length);
    if (writer->position + length > writer->capacity) {
        return -1; // Not enough space
    }
    
    memcpy(writer->buffer + writer->position, data, length);
    writer->position += length;
    
    return 0;
}

int mp_protobuf_read_length_delimited(mp_protobuf_reader_t *reader, uint8_t **data, size_t *length) {
    if (!reader || !length) return -1;
    
    uint64_t len;
    if (mp_protobuf_read_varint(reader, &len) != 0) {
        return -1;
    }
    
    if (len > reader->length - reader->position) {
        return -1; // Not enough data
    }
    
    *length = (size_t)len;
    if (data) {
        *data = (uint8_t *)reader->buffer + reader->position;
    }
    reader->position += len;
    
    return 0;
}

// String encoding/decoding
int mp_protobuf_write_string(mp_protobuf_writer_t *writer, const char *str) {
    if (!writer || !str) return -1;
    return mp_protobuf_write_string_with_length(writer, str, strlen(str));
}

int mp_protobuf_write_string_with_length(mp_protobuf_writer_t *writer, const char *str, size_t length) {
    if (!writer || (!str && length > 0)) return -1;
    return mp_protobuf_write_length_delimited(writer, (const uint8_t *)str, length);
}

int mp_protobuf_read_string(mp_protobuf_reader_t *reader, char **str, size_t *length) {
    if (!reader || !length) return -1;
    
    uint8_t *data;
    if (mp_protobuf_read_length_delimited(reader, &data, length) != 0) {
        return -1;
    }
    
    if (str) {
        // Allocate and copy string
        *str = (char *)malloc(*length + 1);
        if (!*str) {
            return -1;
        }
        memcpy(*str, data, *length);
        (*str)[*length] = '\0';
    }
    
    return 0;
}

// Bytes encoding/decoding
int mp_protobuf_write_bytes(mp_protobuf_writer_t *writer, const uint8_t *data, size_t length) {
    if (!writer || (!data && length > 0)) return -1;
    return mp_protobuf_write_length_delimited(writer, data, length);
}

int mp_protobuf_read_bytes(mp_protobuf_reader_t *reader, uint8_t **data, size_t *length) {
    if (!reader || !length) return -1;
    
    uint8_t *internal_data;
    if (mp_protobuf_read_length_delimited(reader, &internal_data, length) != 0) {
        return -1;
    }
    
    if (data) {
        // Allocate and copy data
        *data = (uint8_t *)malloc(*length);
        if (!*data) {
            return -1;
        }
        memcpy(*data, internal_data, *length);
    }
    
    return 0;
}

// Boolean encoding/decoding
int mp_protobuf_write_bool(mp_protobuf_writer_t *writer, bool value) {
    if (!writer) return -1;
    return mp_protobuf_write_varint(writer, value ? 1 : 0);
}

int mp_protobuf_read_bool(mp_protobuf_reader_t *reader, bool *value) {
    if (!reader || !value) return -1;
    
    uint64_t val;
    if (mp_protobuf_read_varint(reader, &val) != 0) {
        return -1;
    }
    
    *value = val != 0;
    return 0;
}

// Enum encoding/decoding (same as varint)
int mp_protobuf_write_enum(mp_protobuf_writer_t *writer, int32_t value) {
    return mp_protobuf_write_varint(writer, (uint64_t)(uint32_t)value);
}

int mp_protobuf_read_enum(mp_protobuf_reader_t *reader, int32_t *value) {
    if (!reader || !value) return -1;
    
    uint64_t val;
    if (mp_protobuf_read_varint(reader, &val) != 0) {
        return -1;
    }
    
    *value = (int32_t)(uint32_t)val;
    return 0;
}

// Field header encoding/decoding
int mp_protobuf_write_field_header(mp_protobuf_writer_t *writer, uint32_t field_number, mp_protobuf_wire_type_t wire_type) {
    if (!writer) return -1;
    
    // Field number and wire type are combined into a single varint
    uint64_t tag = (field_number << 3) | wire_type;
    return mp_protobuf_write_varint(writer, tag);
}

int mp_protobuf_read_field_header(mp_protobuf_reader_t *reader, uint32_t *field_number, mp_protobuf_wire_type_t *wire_type) {
    if (!reader || !field_number || !wire_type) return -1;
    
    uint64_t tag;
    if (mp_protobuf_read_varint(reader, &tag) != 0) {
        return -1;
    }
    
    *field_number = (uint32_t)(tag >> 3);
    *wire_type = (mp_protobuf_wire_type_t)(tag & 0x07);
    
    return 0;
}

// Double encoding/decoding
int mp_protobuf_write_double(mp_protobuf_writer_t *writer, double value) {
    if (!writer) return -1;
    
    // Convert double to uint64
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    
    return mp_protobuf_write_fixed64(writer, bits);
}

int mp_protobuf_read_double(mp_protobuf_reader_t *reader, double *value) {
    if (!reader || !value) return -1;
    
    uint64_t bits;
    if (mp_protobuf_read_fixed64(reader, &bits) != 0) {
        return -1;
    }
    
    memcpy(value, &bits, sizeof(*value));
    return 0;
}

// Float encoding/decoding
int mp_protobuf_write_float(mp_protobuf_writer_t *writer, float value) {
    if (!writer) return -1;
    
    // Convert float to uint32
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    
    return mp_protobuf_write_fixed32(writer, bits);
}

int mp_protobuf_read_float(mp_protobuf_reader_t *reader, float *value) {
    if (!reader || !value) return -1;
    
    uint32_t bits;
    if (mp_protobuf_read_fixed32(reader, &bits) != 0) {
        return -1;
    }
    
    memcpy(value, &bits, sizeof(*value));
    return 0;
}

// Int32 encoding/decoding
int mp_protobuf_write_int32(mp_protobuf_writer_t *writer, int32_t value) {
    return mp_protobuf_write_varint(writer, (uint64_t)(uint32_t)value);
}

int mp_protobuf_read_int32(mp_protobuf_reader_t *reader, int32_t *value) {
    if (!reader || !value) return -1;
    
    uint64_t val;
    if (mp_protobuf_read_varint(reader, &val) != 0) {
        return -1;
    }
    
    *value = (int32_t)(uint32_t)val;
    return 0;
}

// Int64 encoding/decoding
int mp_protobuf_write_int64(mp_protobuf_writer_t *writer, int64_t value) {
    return mp_protobuf_write_varint(writer, (uint64_t)value);
}

int mp_protobuf_read_int64(mp_protobuf_reader_t *reader, int64_t *value) {
    if (!reader || !value) return -1;
    
    uint64_t val;
    if (mp_protobuf_read_varint(reader, &val) != 0) {
        return -1;
    }
    
    *value = (int64_t)val;
    return 0;
}

// Uint32 encoding/decoding
int mp_protobuf_write_uint32(mp_protobuf_writer_t *writer, uint32_t value) {
    return mp_protobuf_write_varint(writer, (uint64_t)value);
}

int mp_protobuf_read_uint32(mp_protobuf_reader_t *reader, uint32_t *value) {
    if (!reader || !value) return -1;
    
    uint64_t val;
    if (mp_protobuf_read_varint(reader, &val) != 0) {
        return -1;
    }
    
    *value = (uint32_t)val;
    return 0;
}

// Uint64 encoding/decoding
int mp_protobuf_write_uint64(mp_protobuf_writer_t *writer, uint64_t value) {
    return mp_protobuf_write_varint(writer, value);
}

int mp_protobuf_read_uint64(mp_protobuf_reader_t *reader, uint64_t *value) {
    return mp_protobuf_read_varint(reader, value);
}

// Sint32 encoding/decoding (zig-zag)
int mp_protobuf_write_sint32(mp_protobuf_writer_t *writer, int32_t value) {
    return mp_protobuf_write_varint(writer, (uint64_t)(uint32_t)mp_protobuf_zigzag32(value));
}

int mp_protobuf_read_sint32(mp_protobuf_reader_t *reader, int32_t *value) {
    if (!reader || !value) return -1;
    
    uint64_t val;
    if (mp_protobuf_read_varint(reader, &val) != 0) {
        return -1;
    }
    
    *value = mp_protobuf_unzigzag32((uint32_t)val);
    return 0;
}

// Sint64 encoding/decoding (zig-zag)
int mp_protobuf_write_sint64(mp_protobuf_writer_t *writer, int64_t value) {
    return mp_protobuf_write_varint(writer, (uint64_t)mp_protobuf_zigzag64(value));
}

int mp_protobuf_read_sint64(mp_protobuf_reader_t *reader, int64_t *value) {
    if (!reader || !value) return -1;
    
    uint64_t val;
    if (mp_protobuf_read_varint(reader, &val) != 0) {
        return -1;
    }
    
    *value = mp_protobuf_unzigzag64(val);
    return 0;
}

// Sfixed32 encoding/decoding
int mp_protobuf_write_sfixed32(mp_protobuf_writer_t *writer, int32_t value) {
    return mp_protobuf_write_fixed32(writer, (uint32_t)value);
}

int mp_protobuf_read_sfixed32(mp_protobuf_reader_t *reader, int32_t *value) {
    if (!reader || !value) return -1;
    
    uint32_t val;
    if (mp_protobuf_read_fixed32(reader, &val) != 0) {
        return -1;
    }
    
    *value = (int32_t)val;
    return 0;
}

// Sfixed64 encoding/decoding
int mp_protobuf_write_sfixed64(mp_protobuf_writer_t *writer, int64_t value) {
    return mp_protobuf_write_fixed64(writer, (uint64_t)value);
}

int mp_protobuf_read_sfixed64(mp_protobuf_reader_t *reader, int64_t *value) {
    if (!reader || !value) return -1;
    
    uint64_t val;
    if (mp_protobuf_read_fixed64(reader, &val) != 0) {
        return -1;
    }
    
    *value = (int64_t)val;
    return 0;
}

// Skip field
int mp_protobuf_skip_field(mp_protobuf_reader_t *reader, mp_protobuf_wire_type_t wire_type) {
    if (!reader) return -1;
    
    switch (wire_type) {
        case MP_PROTOBUF_VARINT: {
            uint64_t val;
            return mp_protobuf_read_varint(reader, &val);
        }
        case MP_PROTOBUF_FIXED64: {
            uint64_t val;
            return mp_protobuf_read_fixed64(reader, &val);
        }
        case MP_PROTOBUF_LENGTH_DELIMITED: {
            uint64_t len;
            if (mp_protobuf_read_varint(reader, &len) != 0) {
                return -1;
            }
            if (len > reader->length - reader->position) {
                return -1;
            }
            reader->position += len;
            return 0;
        }
        case MP_PROTOBUF_FIXED32: {
            uint32_t val;
            return mp_protobuf_read_fixed32(reader, &val);
        }
        default:
            return -1;
    }
}

int mp_protobuf_skip_unknown(mp_protobuf_reader_t *reader) {
    if (!reader) return -1;
    
    uint32_t field_number;
    mp_protobuf_wire_type_t wire_type;
    
    if (mp_protobuf_read_field_header(reader, &field_number, &wire_type) != 0) {
        return -1;
    }
    
    return mp_protobuf_skip_field(reader, wire_type);
}

// Zig-zag encoding
int32_t mp_protobuf_zigzag32(int32_t n) {
    return (n << 1) ^ (n >> 31);
}

int32_t mp_protobuf_unzigzag32(uint32_t n) {
    return (n >> 1) ^ -(n & 1);
}

int64_t mp_protobuf_zigzag64(int64_t n) {
    return (n << 1) ^ (n >> 63);
}

int64_t mp_protobuf_unzigzag64(uint64_t n) {
    return (n >> 1) ^ -(n & 1);
}

// Size calculation
size_t mp_protobuf_varint_size(uint64_t value) {
    size_t size = 1;
    while (value >= 0x80) {
        value >>= 7;
        size++;
    }
    return size;
}

size_t mp_protobuf_field_header_size(uint32_t field_number, mp_protobuf_wire_type_t wire_type) {
    uint64_t tag = (field_number << 3) | wire_type;
    return mp_protobuf_varint_size(tag);
}

size_t mp_protobuf_length_delimited_size(uint32_t field_number, size_t length) {
    return mp_protobuf_field_header_size(field_number, MP_PROTOBUF_LENGTH_DELIMITED) + 
           mp_protobuf_varint_size(length) + length;
}

// Message encoding/decoding
intptr_t mp_protobuf_message_size(mp_protobuf_message_descriptor_t *descriptor, const void *message) {
    (void)descriptor;
    (void)message;
    // TODO: Implement message size calculation
    return -1;
}

int mp_protobuf_encode_message(mp_protobuf_writer_t *writer, 
                               mp_protobuf_message_descriptor_t *descriptor, 
                               const void *message) {
    (void)writer;
    (void)descriptor;
    (void)message;
    // TODO: Implement message encoding
    return -1;
}

int mp_protobuf_decode_message(mp_protobuf_reader_t *reader,
                               mp_protobuf_message_descriptor_t *descriptor,
                               void *message) {
    (void)reader;
    (void)descriptor;
    (void)message;
    // TODO: Implement message decoding
    return -1;
}

intptr_t mp_protobuf_encode_to_buffer(uint8_t *buffer, size_t capacity,
                                     mp_protobuf_message_descriptor_t *descriptor,
                                     const void *message) {
    mp_protobuf_writer_t writer;
    if (mp_protobuf_writer_init(&writer, buffer, capacity) != 0) {
        return -1;
    }
    
    if (mp_protobuf_encode_message(&writer, descriptor, message) != 0) {
        return -1;
    }
    
    return (intptr_t)writer.position;
}

intptr_t mp_protobuf_decode_from_buffer(const uint8_t *buffer, size_t length,
                                        mp_protobuf_message_descriptor_t *descriptor,
                                        void *message) {
    mp_protobuf_reader_t reader;
    if (mp_protobuf_reader_init(&reader, buffer, length) != 0) {
        return -1;
    }
    
    if (mp_protobuf_decode_message(&reader, descriptor, message) != 0) {
        return -1;
    }
    
    return (intptr_t)reader.position;
}
