#ifndef MICROPOSIX_SERIALIZATION_H
#define MICROPOSIX_SERIALIZATION_H

/**
 * @file serialization.h
 * @brief Unified serialization header
 * 
 * This header provides a unified interface to all serialization features
 * in microPOSIX, including:
 * - JSON parsing and generation
 * - Protocol Buffers encoding and decoding
 * - EDF (Extensible Data Format) encoding and decoding
 * - OData (Open Data Protocol) support
 */

#include "microposix/serialization/json.h"
#include "microposix/serialization/protobuf.h"
#include "microposix/serialization/edf.h"
#include "microposix/serialization/odata.h"

/**
 * @brief Serialization format types
 */
typedef enum {
    MP_SERIALIZATION_JSON,
    MP_SERIALIZATION_PROTOBUF,
    MP_SERIALIZATION_EDF,
    MP_SERIALIZATION_ODATA,
    MP_SERIALIZATION_BINARY,
    MP_SERIALIZATION_HEX
} mp_serialization_format_t;

/**
 * @brief Serialization options
 */
typedef struct {
    mp_serialization_format_t format; ///< Serialization format
    union {
        mp_json_gen_flags_t json_flags; ///< JSON-specific flags
        // Protobuf and EDF options can be added here
    } format_options;
    bool pretty_print; ///< Whether to use pretty printing
    int indent;        ///< Indentation level
} mp_serialization_options_t;

/**
 * @brief Initialize serialization subsystem
 * 
 * @return 0 on success, -1 on error
 */
int mp_serialization_init(void);

/**
 * @brief Shutdown serialization subsystem
 */
void mp_serialization_shutdown(void);

/**
 * @brief Serialize data to a buffer
 * 
 * @param format Serialization format
 * @param data Data to serialize
 * @param size Size of data
 * @param buffer Output buffer
 * @param capacity Buffer capacity
 * @param options Serialization options
 * @return Number of bytes written, or -1 on error
 */
intptr_t mp_serialize(mp_serialization_format_t format, const void *data, size_t size,
                   uint8_t *buffer, size_t capacity, mp_serialization_options_t *options);

/**
 * @brief Deserialize data from a buffer
 * 
 * @param format Serialization format
 * @param buffer Input buffer
 * @param length Buffer length
 * @param data Output buffer for deserialized data
 * @param size Size of output buffer
 * @param options Serialization options
 * @return Number of bytes read, or -1 on error
 */
intptr_t mp_deserialize(mp_serialization_format_t format, const uint8_t *buffer, size_t length,
                     void *data, size_t size, mp_serialization_options_t *options);

/**
 * @brief Get the size needed for serialization
 * 
 * @param format Serialization format
 * @param data Data to serialize
 * @param size Size of data
 * @param options Serialization options
 * @return Size needed in bytes, or -1 on error
 */
intptr_t mp_serialization_size(mp_serialization_format_t format, const void *data, size_t size,
                              mp_serialization_options_t *options);

/**
 * @brief Serialize to a dynamically allocated buffer
 * 
 * @param format Serialization format
 * @param data Data to serialize
 * @param size Size of data
 * @param options Serialization options
 * @param output Output buffer (caller must free)
 * @param length Output length
 * @return 0 on success, -1 on error
 */
int mp_serialize_to_buffer(mp_serialization_format_t format, const void *data, size_t size,
                         mp_serialization_options_t *options, uint8_t **output, size_t *length);

/**
 * @brief Deserialize from a dynamically allocated buffer
 * 
 * @param format Serialization format
 * @param buffer Input buffer
 * @param length Buffer length
 * @param options Serialization options
 * @param data Output buffer for deserialized data
 * @param size Size of output buffer
 * @return 0 on success, -1 on error
 */
int mp_deserialize_from_buffer(mp_serialization_format_t format, const uint8_t *buffer, size_t length,
                              mp_serialization_options_t *options, void *data, size_t size);

/**
 * @brief Serialize to a file
 * 
 * @param format Serialization format
 * @param filename Output filename
 * @param data Data to serialize
 * @param size Size of data
 * @param options Serialization options
 * @return 0 on success, -1 on error
 */
int mp_serialize_to_file(mp_serialization_format_t format, const char *filename,
                       const void *data, size_t size, mp_serialization_options_t *options);

/**
 * @brief Deserialize from a file
 * 
 * @param format Serialization format
 * @param filename Input filename
 * @param options Serialization options
 * @param data Output buffer for deserialized data
 * @param size Size of output buffer
 * @return 0 on success, -1 on error
 */
int mp_deserialize_from_file(mp_serialization_format_t format, const char *filename,
                            mp_serialization_options_t *options, void *data, size_t size);

/**
 * @brief Convert between serialization formats
 * 
 * @param from_format Source format
 * @param to_format Target format
 * @param input Input buffer
 * @param input_length Input length
 * @param output Output buffer
 * @param output_capacity Output capacity
 * @param options Serialization options
 * @return Number of bytes written to output, or -1 on error
 */
intptr_t mp_convert_format(mp_serialization_format_t from_format, mp_serialization_format_t to_format,
                          const uint8_t *input, size_t input_length,
                          uint8_t *output, size_t output_capacity,
                          mp_serialization_options_t *options);

/**
 * @brief Validate serialized data
 * 
 * @param format Serialization format
 * @param buffer Buffer to validate
 * @param length Buffer length
 * @return true if valid
 */
bool mp_serialization_validate(mp_serialization_format_t format, const uint8_t *buffer, size_t length);

/**
 * @brief Get serialization format name
 * 
 * @param format Serialization format
 * @return Format name string
 */
const char *mp_serialization_format_name(mp_serialization_format_t format);

/**
 * @brief Default serialization options for JSON
 * 
 * @return Default JSON options
 */
mp_serialization_options_t mp_serialization_options_json_default(void);

/**
 * @brief Default serialization options for Protocol Buffers
 * 
 * @return Default protobuf options
 */
mp_serialization_options_t mp_serialization_options_protobuf_default(void);

/**
 * @brief Default serialization options for EDF
 * 
 * @return Default EDF options
 */
mp_serialization_options_t mp_serialization_options_edf_default(void);

/**
 * @brief Default serialization options for OData
 * 
 * @return Default OData options
 */
mp_serialization_options_t mp_serialization_options_odata_default(void);

/**
 * @brief Helper macro for JSON serialization
 */
#define mp_json_serialize(data, size, buffer, capacity, flags) \
    mp_serialize(MP_SERIALIZATION_JSON, data, size, buffer, capacity, \
               &(mp_serialization_options_t){.format = MP_SERIALIZATION_JSON, .format_options.json_flags = flags})

/**
 * @brief Helper macro for JSON deserialization
 */
#define mp_json_deserialize(buffer, length, data, size) \
    mp_deserialize(MP_SERIALIZATION_JSON, buffer, length, data, size, NULL)

/**
 * @brief Helper macro for protobuf serialization
 */
#define mp_protobuf_serialize(data, size, buffer, capacity) \
    mp_serialize(MP_SERIALIZATION_PROTOBUF, data, size, buffer, capacity, NULL)

/**
 * @brief Helper macro for protobuf deserialization
 */
#define mp_protobuf_deserialize(buffer, length, data, size) \
    mp_deserialize(MP_SERIALIZATION_PROTOBUF, buffer, length, data, size, NULL)

/**
 * @brief Helper macro for EDF serialization
 */
#define mp_edf_serialize(data, size, buffer, capacity) \
    mp_serialize(MP_SERIALIZATION_EDF, data, size, buffer, capacity, NULL)

/**
 * @brief Helper macro for EDF deserialization
 */
#define mp_edf_deserialize(buffer, length, data, size) \
    mp_deserialize(MP_SERIALIZATION_EDF, buffer, length, data, size, NULL)

/**
 * @brief Helper macro for OData serialization
 */
#define mp_odata_serialize(data, size, buffer, capacity) \
    mp_serialize(MP_SERIALIZATION_ODATA, data, size, buffer, capacity, NULL)

/**
 * @brief Helper macro for OData deserialization
 */
#define mp_odata_deserialize(buffer, length, data, size) \
    mp_deserialize(MP_SERIALIZATION_ODATA, buffer, length, data, size, NULL)

#endif // MICROPOSIX_SERIALIZATION_H
