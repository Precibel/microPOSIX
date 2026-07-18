#ifndef MICROPOSIX_ODATA_H
#define MICROPOSIX_ODATA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "microposix/serialization/json.h"
#include "microposix/mm/memory_view.h"

/**
 * @file odata.h
 * @brief OData (Open Data Protocol) support for microPOSIX
 * 
 * Provides OData v4.0 protocol support for creating and consuming
 * RESTful APIs with query options, entity models, and metadata.
 * 
 * Features:
 * - OData metadata document generation
 * - Entity model definition
 * - Query option parsing ($filter, $select, $orderby, $top, $skip, $count)
 * - OData JSON format support
 * - Entity set operations
 * - Navigation properties
 * - Complex types and collections
 */

// Forward declarations
typedef struct mp_odata_service mp_odata_service_t;
typedef struct mp_odata_entity mp_odata_entity_t;
typedef struct mp_odata_entity_set mp_odata_entity_set_t;
typedef struct mp_odata_property mp_odata_property_t;
typedef struct mp_odata_complex_type mp_odata_complex_type_t;
typedef struct mp_odata_query_options mp_odata_query_options_t;
typedef struct mp_odata_metadata mp_odata_metadata_t;

/**
 * @brief OData data types
 */
typedef enum {
    MP_ODATA_TYPE_STRING,
    MP_ODATA_TYPE_INT32,
    MP_ODATA_TYPE_INT64,
    MP_ODATA_TYPE_DOUBLE,
    MP_ODATA_TYPE_BOOLEAN,
    MP_ODATA_TYPE_DATETIME_OFFSET,
    MP_ODATA_TYPE_DATE,
    MP_ODATA_TYPE_TIME_OF_DAY,
    MP_ODATA_TYPE_DURATION,
    MP_ODATA_TYPE_GUID,
    MP_ODATA_TYPE_BINARY,
    MP_ODATA_TYPE_COMPLEX,
    MP_ODATA_TYPE_ENTITY,
    MP_ODATA_TYPE_COLLECTION,
    MP_ODATA_TYPE_ENUM,
    MP_ODATA_TYPE_NULL
} mp_odata_type_t;

/**
 * @brief OData property structure
 */
struct mp_odata_property {
    const char *name;            ///< Property name
    mp_odata_type_t type;       ///< Property type
    bool nullable;              ///< Whether property can be null
    bool is_key;               ///< Whether property is part of the key
    bool is_computed;          ///< Whether property is computed
    const char *default_value;  ///< Default value (if any)
    size_t max_length;          ///< Maximum length (for strings)
    uint32_t precision;        ///< Precision (for decimals)
    uint32_t scale;            ///< Scale (for decimals)
    bool unicode;              ///< Whether string is unicode
    
    // For complex types and entities
    mp_odata_complex_type_t *complex_type; ///< Complex type definition
    mp_odata_entity_t *entity_type;       ///< Entity type definition
    
    // For navigation properties
    mp_odata_entity_set_t *target_set;     ///< Target entity set
    const char *referential_constraint;    ///< Referential constraint property
    
    // Next in linked list
    mp_odata_property_t *next;
};

/**
 * @brief OData complex type
 */
struct mp_odata_complex_type {
    const char *name;            ///< Type name
    const char *namespace;       ///< Type namespace
    mp_odata_property_t *properties; ///< Properties of this type
    mp_odata_complex_type_t *base_type; ///< Base type (if any)
    bool is_abstract;           ///< Whether type is abstract
    bool is_open;               ///< Whether type is open
    
    // Next in linked list
    mp_odata_complex_type_t *next;
};

/**
 * @brief OData entity type
 */
struct mp_odata_entity {
    const char *name;            ///< Entity name
    const char *namespace;       ///< Entity namespace
    mp_odata_property_t *properties; ///< Properties of this entity
    mp_odata_property_t *key_properties; ///< Key properties
    mp_odata_entity_t *base_type; ///< Base entity type
    bool is_abstract;           ///< Whether entity is abstract
    bool is_open;               ///< Whether entity is open
    const char *has_stream;     ///< Property that contains the stream
    
    // Next in linked list
    mp_odata_entity_t *next;
};

/**
 * @brief OData entity set
 */
struct mp_odata_entity_set {
    const char *name;            ///< Entity set name
    mp_odata_entity_t *entity_type; ///< Entity type
    size_t count;               ///< Number of entities
    void *data;                 ///< Entity data (array of pointers)
    
    // Navigation properties
    mp_odata_entity_set_t **navigation_properties;
    size_t navigation_property_count;
    
    // Next in linked list
    mp_odata_entity_set_t *next;
};

/**
 * @brief OData query options
 */
struct mp_odata_query_options {
    // $filter: Filter expression
    char *filter;
    
    // $select: Properties to include
    char **select;
    size_t select_count;
    
    // $orderby: Sort order
    char **orderby;
    size_t orderby_count;
    
    // $top: Maximum number of results
    int64_t top;
    bool top_set;
    
    // $skip: Number of results to skip
    int64_t skip;
    bool skip_set;
    
    // $count: Include count of results
    bool count;
    bool count_set;
    
    // $expand: Navigation properties to expand
    char **expand;
    size_t expand_count;
    
    // $format: Response format
    char *format;
    
    // $inlinecount: Include inline count
    char *inlinecount;
};

/**
 * @brief OData metadata document
 */
struct mp_odata_metadata {
    const char *service_root;    ///< Service root URL
    mp_odata_entity_set_t *entity_sets; ///< Entity sets
    mp_odata_entity_t *entity_types;    ///< Entity types
    mp_odata_complex_type_t *complex_types; ///< Complex types
    
    // Service document
    mp_json_value_t *service_doc;
    
    // Metadata document
    mp_json_value_t *metadata_doc;
};

/**
 * @brief OData service
 */
struct mp_odata_service {
    const char *name;            ///< Service name
    const char *namespace;       ///< Default namespace
    const char *version;        ///< OData version (e.g., "4.0")
    mp_odata_metadata_t metadata; ///< Service metadata
    
    // Request handling
    void *user_data;            ///< User data for callbacks
    
    // Entity set handlers
    mp_json_value_t *(*get_entity_set)(void *user_data, const char *entity_set_name, mp_odata_query_options_t *options);
    mp_json_value_t *(*get_entity)(void *user_data, const char *entity_set_name, const char *key);
    int (*create_entity)(void *user_data, const char *entity_set_name, mp_json_value_t *entity);
    int (*update_entity)(void *user_data, const char *entity_set_name, const char *key, mp_json_value_t *entity);
    int (*delete_entity)(void *user_data, const char *entity_set_name, const char *key);
    
    // Function imports
    mp_json_value_t *(*call_function)(void *user_data, const char *function_name, mp_json_value_t *parameters);
    
    // Action imports
    mp_json_value_t *(*call_action)(void *user_data, const char *action_name, const char *entity_set_name, const char *key, mp_json_value_t *parameters);
};

/**
 * @brief OData error structure
 */
typedef struct {
    const char *code;           ///< Error code
    const char *message;        ///< Error message
    const char *target;         ///< Error target
    const char *details;       ///< Error details
    mp_json_value_t *inner_error; ///< Inner error (if any)
} mp_odata_error_t;

/**
 * @brief OData response structure
 */
typedef struct {
    mp_json_value_t *data;     ///< Response data
    const char *content_type;   ///< Content type
    uint16_t status_code;      ///< HTTP status code
    mp_odata_error_t *error;   ///< Error (if any)
} mp_odata_response_t;

/**
 * @brief OData context URL builder
 */
typedef struct {
    const char *base_url;       ///< Base URL
    const char *metadata_url;   ///< Metadata URL
    const char *service_root;  ///< Service root URL
} mp_odata_context_t;

/**
 * @brief Initialize an OData service
 * 
 * @param service The service to initialize
 * @param name Service name
 * @param namespace Default namespace
 * @param version OData version
 * @return 0 on success, -1 on error
 */
int mp_odata_service_init(mp_odata_service_t *service, const char *name, const char *namespace, const char *version);

/**
 * @brief Destroy an OData service
 * 
 * @param service The service to destroy
 */
void mp_odata_service_destroy(mp_odata_service_t *service);

/**
 * @brief Add an entity type to the service
 * 
 * @param service The service
 * @param entity_type The entity type to add
 * @return 0 on success, -1 on error
 */
int mp_odata_service_add_entity_type(mp_odata_service_t *service, mp_odata_entity_t *entity_type);

/**
 * @brief Add a complex type to the service
 * 
 * @param service The service
 * @param complex_type The complex type to add
 * @return 0 on success, -1 on error
 */
int mp_odata_service_add_complex_type(mp_odata_service_t *service, mp_odata_complex_type_t *complex_type);

/**
 * @brief Add an entity set to the service
 * 
 * @param service The service
 * @param entity_set The entity set to add
 * @return 0 on success, -1 on error
 */
int mp_odata_service_add_entity_set(mp_odata_service_t *service, mp_odata_entity_set_t *entity_set);

/**
 * @brief Create a new entity type
 * 
 * @param name Entity name
 * @param namespace Entity namespace
 * @return New entity type
 */
mp_odata_entity_t *mp_odata_entity_type_create(const char *name, const char *namespace);

/**
 * @brief Free an entity type
 * 
 * @param entity_type The entity type to free
 */
void mp_odata_entity_type_free(mp_odata_entity_t *entity_type);

/**
 * @brief Add a property to an entity type
 * 
 * @param entity_type The entity type
 * @param name Property name
 * @param type Property type
 * @param is_key Whether property is part of the key
 * @return 0 on success, -1 on error
 */
int mp_odata_entity_type_add_property(mp_odata_entity_t *entity_type, const char *name, mp_odata_type_t type, bool is_key);

/**
 * @brief Create a new complex type
 * 
 * @param name Type name
 * @param namespace Type namespace
 * @return New complex type
 */
mp_odata_complex_type_t *mp_odata_complex_type_create(const char *name, const char *namespace);

/**
 * @brief Free a complex type
 * 
 * @param complex_type The complex type to free
 */
void mp_odata_complex_type_free(mp_odata_complex_type_t *complex_type);

/**
 * @brief Add a property to a complex type
 * 
 * @param complex_type The complex type
 * @param name Property name
 * @param type Property type
 * @return 0 on success, -1 on error
 */
int mp_odata_complex_type_add_property(mp_odata_complex_type_t *complex_type, const char *name, mp_odata_type_t type);

/**
 * @brief Create a new entity set
 * 
 * @param name Entity set name
 * @param entity_type Entity type
 * @return New entity set
 */
mp_odata_entity_set_t *mp_odata_entity_set_create(const char *name, mp_odata_entity_t *entity_type);

/**
 * @brief Free an entity set
 * 
 * @param entity_set The entity set to free
 */
void mp_odata_entity_set_free(mp_odata_entity_set_t *entity_set);

/**
 * @brief Add an entity to an entity set
 * 
 * @param entity_set The entity set
 * @param entity The entity to add (as JSON value)
 * @return 0 on success, -1 on error
 */
int mp_odata_entity_set_add_entity(mp_odata_entity_set_t *entity_set, mp_json_value_t *entity);

/**
 * @brief Parse OData query options from a URL query string
 * 
 * @param query_string The query string (e.g., "?$filter=...&$top=10")
 * @param options Output structure for parsed options
 * @return 0 on success, -1 on error
 */
int mp_odata_parse_query_options(const char *query_string, mp_odata_query_options_t *options);

/**
 * @brief Free query options
 * 
 * @param options The options to free
 */
void mp_odata_query_options_free(mp_odata_query_options_t *options);

/**
 * @brief Generate OData metadata document
 * 
 * @param service The service
 * @return JSON value containing the metadata document
 */
mp_json_value_t *mp_odata_generate_metadata(mp_odata_service_t *service);

/**
 * @brief Generate OData service document
 * 
 * @param service The service
 * @return JSON value containing the service document
 */
mp_json_value_t *mp_odata_generate_service_document(mp_odata_service_t *service);

/**
 * @brief Generate OData context URL
 * 
 * @param context The context
 * @param entity_set_name Entity set name
 * @return Context URL string (caller must free)
 */
char *mp_odata_generate_context_url(mp_odata_context_t *context, const char *entity_set_name);

/**
 * @brief Generate OData @odata.context for an entity
 * 
 * @param context The context
 * @param entity_set_name Entity set name
 * @return JSON value for @odata.context
 */
mp_json_value_t *mp_odata_generate_context(mp_odata_context_t *context, const char *entity_set_name);

/**
 * @brief Generate OData @odata.metadata for an entity
 * 
 * @param context The context
 * @return JSON value for @odata.metadata
 */
mp_json_value_t *mp_odata_generate_metadata_url(mp_odata_context_t *context);

/**
 * @brief Apply $select option to an entity
 * 
 * @param entity The entity to filter
 * @param select Properties to include (NULL-terminated array)
 * @return New JSON value with only selected properties
 */
mp_json_value_t *mp_odata_apply_select(mp_json_value_t *entity, char **select);

/**
 * @brief Apply $filter option to an entity set
 * 
 * @param entities Array of entities
 * @param count Number of entities
 * @param filter Filter expression
 * @param filtered_entities Output array for filtered entities
 * @param filtered_count Output count of filtered entities
 * @return 0 on success, -1 on error
 */
int mp_odata_apply_filter(mp_json_value_t **entities, size_t count, const char *filter, mp_json_value_t ***filtered_entities, size_t *filtered_count);

/**
 * @brief Apply $orderby option to an entity set
 * 
 * @param entities Array of entities
 * @param count Number of entities
 * @param orderby Order by expressions
 * @param orderby_count Number of order by expressions
 * @return 0 on success, -1 on error
 */
int mp_odata_apply_orderby(mp_json_value_t **entities, size_t count, char **orderby, size_t orderby_count);

/**
 * @brief Apply $top and $skip options to an entity set
 * 
 * @param entities Array of entities
 * @param count Number of entities
 * @param top Maximum number of results
 * @param skip Number of results to skip
 * @param result Output array for results
 * @param result_count Output count of results
 * @return 0 on success, -1 on error
 */
int mp_odata_apply_paging(mp_json_value_t **entities, size_t count, int64_t top, int64_t skip, mp_json_value_t ***result, size_t *result_count);

/**
 * @brief Create an OData response
 * 
 * @param data Response data
 * @param content_type Content type
 * @param status_code HTTP status code
 * @return OData response
 */
mp_odata_response_t *mp_odata_response_create(mp_json_value_t *data, const char *content_type, uint16_t status_code);

/**
 * @brief Free an OData response
 * 
 * @param response The response to free
 */
void mp_odata_response_free(mp_odata_response_t *response);

/**
 * @brief Create an OData error response
 * 
 * @param code Error code
 * @param message Error message
 * @param target Error target
 * @param details Error details
 * @param status_code HTTP status code
 * @return OData response with error
 */
mp_odata_response_t *mp_odata_response_create_error(const char *code, const char *message, const char *target, const char *details, uint16_t status_code);

/**
 * @brief Generate a complete OData response
 * 
 * @param service The service
 * @param entity_set_name Entity set name
 * @param entities Array of entities
 * @param count Number of entities
 * @param options Query options
 * @param context OData context
 * @return OData response
 */
mp_odata_response_t *mp_odata_generate_response(mp_odata_service_t *service, const char *entity_set_name, mp_json_value_t **entities, size_t count, mp_odata_query_options_t *options, mp_odata_context_t *context);

/**
 * @brief Generate OData entity response with @odata.context
 * 
 * @param entity The entity
 * @param context OData context
 * @param entity_set_name Entity set name
 * @return JSON value with @odata.context
 */
mp_json_value_t *mp_odata_wrap_entity_with_context(mp_json_value_t *entity, mp_odata_context_t *context, const char *entity_set_name);

/**
 * @brief Generate OData collection response
 * 
 * @param entities Array of entities
 * @param count Number of entities
 * @param context OData context
 * @param entity_set_name Entity set name
 * @param total_count Total count (for $count)
 * @return JSON value with @odata.context and value array
 */
mp_json_value_t *mp_odata_wrap_collection_with_context(mp_json_value_t **entities, size_t count, mp_odata_context_t *context, const char *entity_set_name, int64_t total_count);

/**
 * @brief OData type names
 */
#define MP_ODATA_TYPE_STRING_NAME "Edm.String"
#define MP_ODATA_TYPE_INT32_NAME "Edm.Int32"
#define MP_ODATA_TYPE_INT64_NAME "Edm.Int64"
#define MP_ODATA_TYPE_DOUBLE_NAME "Edm.Double"
#define MP_ODATA_TYPE_BOOLEAN_NAME "Edm.Boolean"
#define MP_ODATA_TYPE_DATETIME_OFFSET_NAME "Edm.DateTimeOffset"
#define MP_ODATA_TYPE_DATE_NAME "Edm.Date"
#define MP_ODATA_TYPE_TIME_OF_DAY_NAME "Edm.TimeOfDay"
#define MP_ODATA_TYPE_DURATION_NAME "Edm.Duration"
#define MP_ODATA_TYPE_GUID_NAME "Edm.Guid"
#define MP_ODATA_TYPE_BINARY_NAME "Edm.Binary"

/**
 * @brief Get OData type name from type enum
 * 
 * @param type The type
 * @return Type name string
 */
const char *mp_odata_get_type_name(mp_odata_type_t type);

/**
 * @brief Convert JSON value to OData type
 * 
 * @param value JSON value
 * @return OData type
 */
mp_odata_type_t mp_odata_get_type_from_json(mp_json_value_t *value);

/**
 * @brief Create a simple OData entity
 * 
 * @param entity_set_name Entity set name
 * @param key Entity key
 * @return JSON value for the entity
 */
mp_json_value_t *mp_odata_create_entity(const char *entity_set_name, const char *key);

/**
 * @brief Add a property to an OData entity
 * 
 * @param entity The entity
 * @param name Property name
 * @param value Property value (as JSON)
 */
void mp_odata_entity_add_property(mp_json_value_t *entity, const char *name, mp_json_value_t *value);

/**
 * @brief Helper macro to define an OData entity type
 */
#define MP_ODATA_ENTITY_TYPE(name, namespace) \
    static mp_odata_entity_t name##_entity_type = { \
        .name = #name, \
        .namespace = namespace, \
        .properties = NULL, \
        .key_properties = NULL, \
        .base_type = NULL, \
        .is_abstract = false, \
        .is_open = false, \
        .has_stream = NULL, \
        .next = NULL \
    }

/**
 * @brief Helper macro to add a property to an entity type
 */
#define MP_ODATA_ENTITY_PROPERTY(entity_type, name, type, is_key) \
    do { \
        mp_odata_entity_type_add_property(&entity_type, #name, type, is_key); \
    } while (0)

/**
 * @brief Helper macro to define an OData complex type
 */
#define MP_ODATA_COMPLEX_TYPE(name, namespace) \
    static mp_odata_complex_type_t name##_complex_type = { \
        .name = #name, \
        .namespace = namespace, \
        .properties = NULL, \
        .base_type = NULL, \
        .is_abstract = false, \
        .is_open = false, \
        .next = NULL \
    }

/**
 * @brief Helper macro to add a property to a complex type
 */
#define MP_ODATA_COMPLEX_PROPERTY(complex_type, name, type) \
    do { \
        mp_odata_complex_type_add_property(&complex_type, #name, type); \
    } while (0)

/**
 * @brief Helper macro to define an OData entity set
 */
#define MP_ODATA_ENTITY_SET(name, entity_type) \
    static mp_odata_entity_set_t name##_entity_set = { \
        .name = #name, \
        .entity_type = &entity_type, \
        .count = 0, \
        .data = NULL, \
        .navigation_properties = NULL, \
        .navigation_property_count = 0, \
        .next = NULL \
    }

#endif // MICROPOSIX_ODATA_H
