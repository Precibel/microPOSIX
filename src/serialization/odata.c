#include "microposix/serialization/odata.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

// Helper to create a JSON string value
static mp_json_value_t *create_json_string(const char *value) {
    if (!value) {
        return mp_json_null();
    }
    return mp_json_string(value);
}

// Helper to create a JSON boolean value
static mp_json_value_t *create_json_boolean(bool value) {
    return mp_json_boolean(value);
}

// Helper to create a JSON number value
static mp_json_value_t *create_json_number(double value) {
    return mp_json_number(value);
}

// Helper to create a JSON integer value
static mp_json_value_t *create_json_integer(int64_t value) {
    return mp_json_integer(value);
}

// Initialize OData service
int mp_odata_service_init(mp_odata_service_t *service, const char *name, const char *namespace, const char *version) {
    if (!service) {
        return -1;
    }
    
    service->name = name ? strdup(name) : NULL;
    service->namespace = namespace ? strdup(namespace) : NULL;
    service->version = version ? strdup(version) : strdup("4.0");
    
    memset(&service->metadata, 0, sizeof(service->metadata));
    service->metadata.service_root = NULL;
    service->metadata.entity_sets = NULL;
    service->metadata.entity_types = NULL;
    service->metadata.complex_types = NULL;
    service->metadata.service_doc = NULL;
    service->metadata.metadata_doc = NULL;
    
    service->user_data = NULL;
    service->get_entity_set = NULL;
    service->get_entity = NULL;
    service->create_entity = NULL;
    service->update_entity = NULL;
    service->delete_entity = NULL;
    service->call_function = NULL;
    service->call_action = NULL;
    
    return 0;
}

void mp_odata_service_destroy(mp_odata_service_t *service) {
    if (!service) {
        return;
    }
    
    if (service->name) {
        free((void *)service->name);
    }
    if (service->namespace) {
        free((void *)service->namespace);
    }
    if (service->version) {
        free((void *)service->version);
    }
    
    // Free entity sets
    mp_odata_entity_set_t *entity_set = service->metadata.entity_sets;
    while (entity_set) {
        mp_odata_entity_set_t *next = entity_set->next;
        mp_odata_entity_set_free(entity_set);
        entity_set = next;
    }
    
    // Free entity types
    mp_odata_entity_t *entity_type = service->metadata.entity_types;
    while (entity_type) {
        mp_odata_entity_t *next = entity_type->next;
        mp_odata_entity_type_free(entity_type);
        entity_type = next;
    }
    
    // Free complex types
    mp_odata_complex_type_t *complex_type = service->metadata.complex_types;
    while (complex_type) {
        mp_odata_complex_type_t *next = complex_type->next;
        mp_odata_complex_type_free(complex_type);
        complex_type = next;
    }
    
    if (service->metadata.service_doc) {
        mp_json_free(service->metadata.service_doc);
    }
    if (service->metadata.metadata_doc) {
        mp_json_free(service->metadata.metadata_doc);
    }
    
    memset(service, 0, sizeof(*service));
}

int mp_odata_service_add_entity_type(mp_odata_service_t *service, mp_odata_entity_t *entity_type) {
    if (!service || !entity_type) {
        return -1;
    }
    
    // Add to linked list
    entity_type->next = service->metadata.entity_types;
    service->metadata.entity_types = entity_type;
    
    return 0;
}

int mp_odata_service_add_complex_type(mp_odata_service_t *service, mp_odata_complex_type_t *complex_type) {
    if (!service || !complex_type) {
        return -1;
    }
    
    // Add to linked list
    complex_type->next = service->metadata.complex_types;
    service->metadata.complex_types = complex_type;
    
    return 0;
}

int mp_odata_service_add_entity_set(mp_odata_service_t *service, mp_odata_entity_set_t *entity_set) {
    if (!service || !entity_set) {
        return -1;
    }
    
    // Add to linked list
    entity_set->next = service->metadata.entity_sets;
    service->metadata.entity_sets = entity_set;
    
    return 0;
}

mp_odata_entity_t *mp_odata_entity_type_create(const char *name, const char *namespace) {
    mp_odata_entity_t *entity_type = (mp_odata_entity_t *)malloc(sizeof(mp_odata_entity_t));
    if (!entity_type) {
        return NULL;
    }
    
    entity_type->name = name ? strdup(name) : NULL;
    entity_type->namespace = namespace ? strdup(namespace) : NULL;
    entity_type->properties = NULL;
    entity_type->key_properties = NULL;
    entity_type->base_type = NULL;
    entity_type->is_abstract = false;
    entity_type->is_open = false;
    entity_type->has_stream = NULL;
    entity_type->next = NULL;
    
    return entity_type;
}

void mp_odata_entity_type_free(mp_odata_entity_t *entity_type) {
    if (!entity_type) {
        return;
    }
    
    if (entity_type->name) {
        free((void *)entity_type->name);
    }
    if (entity_type->namespace) {
        free((void *)entity_type->namespace);
    }
    
    // Free properties
    mp_odata_property_t *prop = entity_type->properties;
    while (prop) {
        mp_odata_property_t *next = prop->next;
        free(prop);
        prop = next;
    }
    
    free(entity_type);
}

int mp_odata_entity_type_add_property(mp_odata_entity_t *entity_type, const char *name, mp_odata_type_t type, bool is_key) {
    if (!entity_type || !name) {
        return -1;
    }
    
    mp_odata_property_t *prop = (mp_odata_property_t *)malloc(sizeof(mp_odata_property_t));
    if (!prop) {
        return -1;
    }
    
    prop->name = strdup(name);
    prop->type = type;
    prop->nullable = true;
    prop->is_key = is_key;
    prop->is_computed = false;
    prop->default_value = NULL;
    prop->max_length = 0;
    prop->precision = 0;
    prop->scale = 0;
    prop->unicode = true;
    prop->complex_type = NULL;
    prop->entity_type = NULL;
    prop->target_set = NULL;
    prop->referential_constraint = NULL;
    prop->next = NULL;
    
    // Add to properties list
    prop->next = entity_type->properties;
    entity_type->properties = prop;
    
    // If it's a key property, add to key properties list
    if (is_key) {
        prop->next = entity_type->key_properties;
        entity_type->key_properties = prop;
    }
    
    return 0;
}

mp_odata_complex_type_t *mp_odata_complex_type_create(const char *name, const char *namespace) {
    mp_odata_complex_type_t *complex_type = (mp_odata_complex_type_t *)malloc(sizeof(mp_odata_complex_type_t));
    if (!complex_type) {
        return NULL;
    }
    
    complex_type->name = name ? strdup(name) : NULL;
    complex_type->namespace = namespace ? strdup(namespace) : NULL;
    complex_type->properties = NULL;
    complex_type->base_type = NULL;
    complex_type->is_abstract = false;
    complex_type->is_open = false;
    complex_type->next = NULL;
    
    return complex_type;
}

void mp_odata_complex_type_free(mp_odata_complex_type_t *complex_type) {
    if (!complex_type) {
        return;
    }
    
    if (complex_type->name) {
        free((void *)complex_type->name);
    }
    if (complex_type->namespace) {
        free((void *)complex_type->namespace);
    }
    
    // Free properties
    mp_odata_property_t *prop = complex_type->properties;
    while (prop) {
        mp_odata_property_t *next = prop->next;
        free(prop);
        prop = next;
    }
    
    free(complex_type);
}

int mp_odata_complex_type_add_property(mp_odata_complex_type_t *complex_type, const char *name, mp_odata_type_t type) {
    if (!complex_type || !name) {
        return -1;
    }
    
    mp_odata_property_t *prop = (mp_odata_property_t *)malloc(sizeof(mp_odata_property_t));
    if (!prop) {
        return -1;
    }
    
    prop->name = strdup(name);
    prop->type = type;
    prop->nullable = true;
    prop->is_key = false;
    prop->is_computed = false;
    prop->default_value = NULL;
    prop->max_length = 0;
    prop->precision = 0;
    prop->scale = 0;
    prop->unicode = true;
    prop->complex_type = NULL;
    prop->entity_type = NULL;
    prop->target_set = NULL;
    prop->referential_constraint = NULL;
    prop->next = NULL;
    
    // Add to properties list
    prop->next = complex_type->properties;
    complex_type->properties = prop;
    
    return 0;
}

mp_odata_entity_set_t *mp_odata_entity_set_create(const char *name, mp_odata_entity_t *entity_type) {
    mp_odata_entity_set_t *entity_set = (mp_odata_entity_set_t *)malloc(sizeof(mp_odata_entity_set_t));
    if (!entity_set) {
        return NULL;
    }
    
    entity_set->name = name ? strdup(name) : NULL;
    entity_set->entity_type = entity_type;
    entity_set->count = 0;
    entity_set->data = NULL;
    entity_set->navigation_properties = NULL;
    entity_set->navigation_property_count = 0;
    entity_set->next = NULL;
    
    return entity_set;
}

void mp_odata_entity_set_free(mp_odata_entity_set_t *entity_set) {
    if (!entity_set) {
        return;
    }
    
    if (entity_set->name) {
        free((void *)entity_set->name);
    }
    
    // Free data array
    if (entity_set->data) {
        free(entity_set->data);
    }
    
    // Free navigation properties
    if (entity_set->navigation_properties) {
        free(entity_set->navigation_properties);
    }
    
    free(entity_set);
}

int mp_odata_entity_set_add_entity(mp_odata_entity_set_t *entity_set, mp_json_value_t *entity) {
    if (!entity_set || !entity) {
        return -1;
    }
    
    // Reallocate data array
    void **new_data = (void **)realloc(entity_set->data, (entity_set->count + 1) * sizeof(void *));
    if (!new_data) {
        return -1;
    }
    
    entity_set->data = new_data;
    entity_set->data[entity_set->count] = entity;
    entity_set->count++;
    
    return 0;
}

// Parse query string into key-value pairs
static int parse_query_pairs(const char *query_string, char ***keys, char ***values, size_t *count) {
    if (!query_string || !*query_string) {
        *keys = NULL;
        *values = NULL;
        *count = 0;
        return 0;
    }
    
    // Count the number of parameters
    size_t param_count = 0;
    const char *p = query_string;
    while (*p) {
        if (*p == '&') {
            param_count++;
        }
        p++;
    }
    param_count++; // Last parameter
    
    if (param_count == 0) {
        *keys = NULL;
        *values = NULL;
        *count = 0;
        return 0;
    }
    
    // Allocate arrays
    char **k = (char **)malloc(param_count * sizeof(char *));
    char **v = (char **)malloc(param_count * sizeof(char *));
    if (!k || !v) {
        free(k);
        free(v);
        return -1;
    }
    
    // Parse parameters
    size_t i = 0;
    p = query_string;
    while (*p && i < param_count) {
        // Skip leading '&' or '?'
        if (*p == '&' || *p == '?') {
            p++;
        }
        
        // Find end of key
        const char *key_start = p;
        while (*p && *p != '=' && *p != '&') {
            p++;
        }
        
        if (*p == '=') {
            // Parse value
            const char *value_start = p + 1;
            while (*p && *p != '&') {
                p++;
            }
            
            size_t key_len = p - key_start;
            size_t value_len = p - value_start;
            
            k[i] = (char *)malloc(key_len + 1);
            v[i] = (char *)malloc(value_len + 1);
            
            if (!k[i] || !v[i]) {
                // Cleanup
                for (size_t j = 0; j <= i; j++) {
                    free(k[j]);
                    free(v[j]);
                }
                free(k);
                free(v);
                return -1;
            }
            
            memcpy(k[i], key_start, key_len);
            k[i][key_len] = '\0';
            
            memcpy(v[i], value_start, value_len);
            v[i][value_len] = '\0';
            
            i++;
        } else {
            // No value, just a key
            size_t key_len = p - key_start;
            k[i] = (char *)malloc(key_len + 1);
            v[i] = strdup("");
            
            if (!k[i] || !v[i]) {
                for (size_t j = 0; j <= i; j++) {
                    free(k[j]);
                    free(v[j]);
                }
                free(k);
                free(v);
                return -1;
            }
            
            memcpy(k[i], key_start, key_len);
            k[i][key_len] = '\0';
            i++;
        }
    }
    
    *keys = k;
    *values = v;
    *count = i;
    
    return 0;
}

// Free query pairs
static void free_query_pairs(char **keys, char **values, size_t count) {
    if (keys) {
        for (size_t i = 0; i < count; i++) {
            free(keys[i]);
        }
        free(keys);
    }
    if (values) {
        for (size_t i = 0; i < count; i++) {
            free(values[i]);
        }
        free(values);
    }
}

int mp_odata_parse_query_options(const char *query_string, mp_odata_query_options_t *options) {
    if (!options) {
        return -1;
    }
    
    // Initialize options
    memset(options, 0, sizeof(*options));
    options->filter = NULL;
    options->select = NULL;
    options->select_count = 0;
    options->orderby = NULL;
    options->orderby_count = 0;
    options->top = 0;
    options->top_set = false;
    options->skip = 0;
    options->skip_set = false;
    options->count = false;
    options->count_set = false;
    options->expand = NULL;
    options->expand_count = 0;
    options->format = NULL;
    options->inlinecount = NULL;
    
    if (!query_string || !*query_string) {
        return 0;
    }
    
    // Parse query string
    char **keys = NULL;
    char **values = NULL;
    size_t count = 0;
    
    if (parse_query_pairs(query_string, &keys, &values, &count) != 0) {
        return -1;
    }
    
    // Process each parameter
    for (size_t i = 0; i < count; i++) {
        if (strcmp(keys[i], "$filter") == 0) {
            options->filter = strdup(values[i]);
        } else if (strcmp(keys[i], "$select") == 0) {
            // Split select by comma
            char *p = values[i];
            char *token = strtok(p, ",");
            while (token) {
                options->select = (char **)realloc(options->select, (options->select_count + 1) * sizeof(char *));
                if (!options->select) {
                    free_query_pairs(keys, values, count);
                    return -1;
                }
                options->select[options->select_count] = strdup(token);
                if (!options->select[options->select_count]) {
                    free_query_pairs(keys, values, count);
                    return -1;
                }
                options->select_count++;
                token = strtok(NULL, ",");
            }
        } else if (strcmp(keys[i], "$orderby") == 0) {
            // Split orderby by comma
            char *p = strdup(values[i]);
            char *token = strtok(p, ",");
            while (token) {
                options->orderby = (char **)realloc(options->orderby, (options->orderby_count + 1) * sizeof(char *));
                if (!options->orderby) {
                    free(p);
                    free_query_pairs(keys, values, count);
                    return -1;
                }
                options->orderby[options->orderby_count] = strdup(token);
                if (!options->orderby[options->orderby_count]) {
                    free(p);
                    free_query_pairs(keys, values, count);
                    return -1;
                }
                options->orderby_count++;
                token = strtok(NULL, ",");
            }
            free(p);
        } else if (strcmp(keys[i], "$top") == 0) {
            options->top = atoll(values[i]);
            options->top_set = true;
        } else if (strcmp(keys[i], "$skip") == 0) {
            options->skip = atoll(values[i]);
            options->skip_set = true;
        } else if (strcmp(keys[i], "$count") == 0) {
            options->count = strcmp(values[i], "true") == 0;
            options->count_set = true;
        } else if (strcmp(keys[i], "$expand") == 0) {
            // Split expand by comma
            char *p = strdup(values[i]);
            char *token = strtok(p, ",");
            while (token) {
                options->expand = (char **)realloc(options->expand, (options->expand_count + 1) * sizeof(char *));
                if (!options->expand) {
                    free(p);
                    free_query_pairs(keys, values, count);
                    return -1;
                }
                options->expand[options->expand_count] = strdup(token);
                if (!options->expand[options->expand_count]) {
                    free(p);
                    free_query_pairs(keys, values, count);
                    return -1;
                }
                options->expand_count++;
                token = strtok(NULL, ",");
            }
            free(p);
        } else if (strcmp(keys[i], "$format") == 0) {
            options->format = strdup(values[i]);
        } else if (strcmp(keys[i], "$inlinecount") == 0) {
            options->inlinecount = strdup(values[i]);
        }
    }
    
    free_query_pairs(keys, values, count);
    
    return 0;
}

void mp_odata_query_options_free(mp_odata_query_options_t *options) {
    if (!options) {
        return;
    }
    
    if (options->filter) {
        free(options->filter);
    }
    
    for (size_t i = 0; i < options->select_count; i++) {
        free(options->select[i]);
    }
    free(options->select);
    
    for (size_t i = 0; i < options->orderby_count; i++) {
        free(options->orderby[i]);
    }
    free(options->orderby);
    
    for (size_t i = 0; i < options->expand_count; i++) {
        free(options->expand[i]);
    }
    free(options->expand);
    
    if (options->format) {
        free(options->format);
    }
    if (options->inlinecount) {
        free(options->inlinecount);
    }
    
    memset(options, 0, sizeof(*options));
}

// Generate OData metadata document
mp_json_value_t *mp_odata_generate_metadata(mp_odata_service_t *service) {
    if (!service) {
        return NULL;
    }
    
    mp_json_value_t *metadata = mp_json_object();
    if (!metadata) {
        return NULL;
    }
    
    // Add $Version
    mp_json_object_add(metadata, "$Version", create_json_string(service->version));
    
    // Add $Metadata
    mp_json_value_t *metadata_obj = mp_json_object();
    
    // Add EntityTypes
    mp_json_value_t *entity_types = mp_json_array();
    mp_odata_entity_t *entity_type = service->metadata.entity_types;
    while (entity_type) {
        mp_json_value_t *et = mp_json_object();
        mp_json_object_add(et, "Name", create_json_string(entity_type->name));
        mp_json_object_add(et, "Namespace", create_json_string(entity_type->namespace));
        
        // Add Key
        if (entity_type->key_properties) {
            mp_json_value_t *key = mp_json_object();
            mp_odata_property_t *prop = entity_type->key_properties;
            while (prop) {
                mp_json_object_add(key, prop->name, create_json_string(prop->name));
                prop = prop->next;
            }
            mp_json_object_add(et, "Key", key);
        }
        
        // Add Properties
        mp_json_value_t *properties = mp_json_array();
        mp_odata_property_t *prop = entity_type->properties;
        while (prop) {
            mp_json_value_t *p = mp_json_object();
            mp_json_object_add(p, "Name", create_json_string(prop->name));
            mp_json_object_add(p, "Type", create_json_string(mp_odata_get_type_name(prop->type)));
            mp_json_object_add(p, "Nullable", create_json_boolean(prop->nullable));
            
            mp_json_array_add(properties, p);
            prop = prop->next;
        }
        mp_json_object_add(et, "Properties", properties);
        
        mp_json_array_add(entity_types, et);
        entity_type = entity_type->next;
    }
    mp_json_object_add(metadata_obj, "EntityTypes", entity_types);
    
    // Add ComplexTypes
    mp_json_value_t *complex_types = mp_json_array();
    mp_odata_complex_type_t *complex_type = service->metadata.complex_types;
    while (complex_type) {
        mp_json_value_t *ct = mp_json_object();
        mp_json_object_add(ct, "Name", create_json_string(complex_type->name));
        mp_json_object_add(ct, "Namespace", create_json_string(complex_type->namespace));
        
        // Add Properties
        mp_json_value_t *properties = mp_json_array();
        mp_odata_property_t *prop = complex_type->properties;
        while (prop) {
            mp_json_value_t *p = mp_json_object();
            mp_json_object_add(p, "Name", create_json_string(prop->name));
            mp_json_object_add(p, "Type", create_json_string(mp_odata_get_type_name(prop->type)));
            mp_json_object_add(p, "Nullable", create_json_boolean(prop->nullable));
            
            mp_json_array_add(properties, p);
            prop = prop->next;
        }
        mp_json_object_add(ct, "Properties", properties);
        
        mp_json_array_add(complex_types, ct);
        complex_type = complex_type->next;
    }
    mp_json_object_add(metadata_obj, "ComplexTypes", complex_types);
    
    // Add EntitySets
    mp_json_value_t *entity_sets = mp_json_array();
    mp_odata_entity_set_t *entity_set = service->metadata.entity_sets;
    while (entity_set) {
        mp_json_value_t *es = mp_json_object();
        mp_json_object_add(es, "Name", create_json_string(entity_set->name));
        mp_json_object_add(es, "EntityType", create_json_string(entity_set->entity_type->name));
        
        mp_json_array_add(entity_sets, es);
        entity_set = entity_set->next;
    }
    mp_json_object_add(metadata_obj, "EntitySets", entity_sets);
    
    mp_json_object_add(metadata, "$Metadata", metadata_obj);
    
    return metadata;
}

// Generate OData service document
mp_json_value_t *mp_odata_generate_service_document(mp_odata_service_t *service) {
    if (!service) {
        return NULL;
    }
    
    mp_json_value_t *service_doc = mp_json_object();
    if (!service_doc) {
        return NULL;
    }
    
    // Add @odata.context
    mp_json_object_add(service_doc, "@odata.context", create_json_string("$metadata#$service"));
    
    // Add value array
    mp_json_value_t *value = mp_json_array();
    mp_odata_entity_set_t *entity_set = service->metadata.entity_sets;
    while (entity_set) {
        mp_json_value_t *es = mp_json_object();
        mp_json_object_add(es, "name", create_json_string(entity_set->name));
        mp_json_object_add(es, "kind", create_json_string("EntitySet"));
        mp_json_object_add(es, "url", create_json_string(entity_set->name));
        
        mp_json_array_add(value, es);
        entity_set = entity_set->next;
    }
    
    mp_json_object_add(service_doc, "value", value);
    
    return service_doc;
}

char *mp_odata_generate_context_url(mp_odata_context_t *context, const char *entity_set_name) {
    if (!context || !entity_set_name) {
        return NULL;
    }
    
    // Calculate required length
    size_t len = strlen(context->base_url) + strlen(entity_set_name) + 32;
    char *url = (char *)malloc(len);
    if (!url) {
        return NULL;
    }
    
    snprintf(url, len, "%s/%s", context->base_url, entity_set_name);
    return url;
}

mp_json_value_t *mp_odata_generate_context(mp_odata_context_t *context, const char *entity_set_name) {
    if (!context || !entity_set_name) {
        return NULL;
    }
    
    char *url = mp_odata_generate_context_url(context, entity_set_name);
    if (!url) {
        return NULL;
    }
    
    mp_json_value_t *ctx = mp_json_string(url);
    free(url);
    return ctx;
}

mp_json_value_t *mp_odata_generate_metadata_url(mp_odata_context_t *context) {
    if (!context || !context->metadata_url) {
        return NULL;
    }
    
    return mp_json_string(context->metadata_url);
}

mp_json_value_t *mp_odata_apply_select(mp_json_value_t *entity, char **select) {
    if (!entity || !mp_json_is_object(entity)) {
        return entity;
    }
    
    mp_json_value_t *result = mp_json_object();
    if (!result) {
        return entity;
    }
    
    // If no select, return original
    if (!select || !*select) {
        mp_json_free(result);
        return entity;
    }
    
    // Add @odata.context if present
    mp_json_value_t *context = mp_json_object_get(entity, "@odata.context");
    if (context) {
        mp_json_object_add(result, "@odata.context", mp_json_deep_copy(context));
    }
    
    // Add selected properties
    for (char **sel = select; *sel; sel++) {
        mp_json_value_t *value = mp_json_object_get(entity, *sel);
        if (value) {
            mp_json_object_add(result, *sel, mp_json_deep_copy(value));
        }
    }
    
    return result;
}

// Simple filter evaluation (supports basic equality and comparison)
static bool evaluate_filter(mp_json_value_t *entity, const char *filter) {
    if (!entity || !filter || !*filter) {
        return true;
    }
    
    // Parse simple filter: Property op Value
    // Supported: eq, ne, gt, ge, lt, le
    char *p = strdup(filter);
    char *property = strtok(p, " ");
    char *op = strtok(NULL, " ");
    char *value_str = strtok(NULL, "");
    
    if (!property || !op || !value_str) {
        free(p);
        return true;
    }
    
    // Get property value from entity
    mp_json_value_t *prop_value = mp_json_object_get(entity, property);
    if (!prop_value) {
        free(p);
        return false;
    }
    
    // Parse value
    mp_json_value_t *value = NULL;
    if (strcmp(value_str, "true") == 0) {
        value = mp_json_boolean(true);
    } else if (strcmp(value_str, "false") == 0) {
        value = mp_json_boolean(false);
    } else if (value_str[0] == '\'') {
        // String - remove quotes
        value = mp_json_string(value_str + 1);
        size_t len = strlen(value_str);
        if (len > 0 && value_str[len - 1] == '\'') {
            ((mp_json_string_t *)value->data.string.value)[value->data.string.length - 1] = '\0';
        }
    } else {
        // Number
        value = mp_json_number(atof(value_str));
    }
    
    bool result = false;
    
    if (strcmp(op, "eq") == 0) {
        result = mp_json_equal(prop_value, value);
    } else if (strcmp(op, "ne") == 0) {
        result = !mp_json_equal(prop_value, value);
    } else if (strcmp(op, "gt") == 0) {
        if (mp_json_is_number(prop_value) && mp_json_is_number(value)) {
            result = mp_json_get_number(prop_value) > mp_json_get_number(value);
        }
    } else if (strcmp(op, "ge") == 0) {
        if (mp_json_is_number(prop_value) && mp_json_is_number(value)) {
            result = mp_json_get_number(prop_value) >= mp_json_get_number(value);
        }
    } else if (strcmp(op, "lt") == 0) {
        if (mp_json_is_number(prop_value) && mp_json_is_number(value)) {
            result = mp_json_get_number(prop_value) < mp_json_get_number(value);
        }
    } else if (strcmp(op, "le") == 0) {
        if (mp_json_is_number(prop_value) && mp_json_is_number(value)) {
            result = mp_json_get_number(prop_value) <= mp_json_get_number(value);
        }
    }
    
    free(p);
    mp_json_free(value);
    
    return result;
}

int mp_odata_apply_filter(mp_json_value_t **entities, size_t count, const char *filter, mp_json_value_t ***filtered_entities, size_t *filtered_count) {
    if (!entities || count == 0 || !filtered_entities || !filtered_count) {
        return -1;
    }
    
    // Allocate result array
    mp_json_value_t **result = (mp_json_value_t **)malloc(count * sizeof(mp_json_value_t *));
    if (!result) {
        return -1;
    }
    
    size_t result_count = 0;
    
    for (size_t i = 0; i < count; i++) {
        if (evaluate_filter(entities[i], filter)) {
            result[result_count++] = entities[i];
        }
    }
    
    *filtered_entities = result;
    *filtered_count = result_count;
    
    return 0;
}

// Simple comparison for ordering
static int compare_entities(mp_json_value_t *a, mp_json_value_t *b, const char *property, bool descending) {
    mp_json_value_t *a_val = mp_json_object_get(a, property);
    mp_json_value_t *b_val = mp_json_object_get(b, property);
    
    if (!a_val || !b_val) {
        return 0;
    }
    
    if (mp_json_is_number(a_val) && mp_json_is_number(b_val)) {
        double a_num = mp_json_get_number(a_val);
        double b_num = mp_json_get_number(b_val);
        if (a_num < b_num) {
            return descending ? 1 : -1;
        } else if (a_num > b_num) {
            return descending ? -1 : 1;
        }
        return 0;
    } else if (mp_json_is_string(a_val) && mp_json_is_string(b_val)) {
        const char *a_str = mp_json_get_string(a_val);
        const char *b_str = mp_json_get_string(b_val);
        int cmp = strcmp(a_str, b_str);
        return descending ? -cmp : cmp;
    }
    
    return 0;
}

int mp_odata_apply_orderby(mp_json_value_t **entities, size_t count, char **orderby, size_t orderby_count) {
    if (!entities || count == 0 || !orderby || orderby_count == 0) {
        return 0;
    }
    
    // Simple bubble sort (for demonstration)
    for (size_t i = 0; i < count - 1; i++) {
        for (size_t j = 0; j < count - i - 1; j++) {
            bool should_swap = false;
            
            for (size_t k = 0; k < orderby_count; k++) {
                // Parse orderby: Property [asc|desc]
                char *order = orderby[k];
                bool descending = false;
                
                if (strstr(order, " desc")) {
                    descending = true;
                }
                
                // Extract property name
                char *space = strchr(order, ' ');
                size_t prop_len = space ? space - order : strlen(order);
                char property[256];
                strncpy(property, order, prop_len);
                property[prop_len] = '\0';
                
                int cmp = compare_entities(entities[j], entities[j + 1], property, descending);
                if (cmp > 0) {
                    should_swap = true;
                    break;
                } else if (cmp < 0) {
                    should_swap = false;
                    break;
                }
            }
            
            if (should_swap) {
                mp_json_value_t *tmp = entities[j];
                entities[j] = entities[j + 1];
                entities[j + 1] = tmp;
            }
        }
    }
    
    return 0;
}

int mp_odata_apply_paging(mp_json_value_t **entities, size_t count, int64_t top, int64_t skip, mp_json_value_t ***result, size_t *result_count) {
    if (!entities || count == 0 || !result || !result_count) {
        return -1;
    }
    
    // Apply skip
    size_t start = (size_t)skip;
    if (start >= count) {
        *result = NULL;
        *result_count = 0;
        return 0;
    }
    
    // Apply top
    size_t end = count;
    if (top > 0) {
        end = start + (size_t)top;
        if (end > count) {
            end = count;
        }
    }
    
    size_t result_size = end - start;
    mp_json_value_t **result_array = (mp_json_value_t **)malloc(result_size * sizeof(mp_json_value_t *));
    if (!result_array) {
        return -1;
    }
    
    for (size_t i = start; i < end; i++) {
        result_array[i - start] = entities[i];
    }
    
    *result = result_array;
    *result_count = result_size;
    
    return 0;
}

mp_odata_response_t *mp_odata_response_create(mp_json_value_t *data, const char *content_type, uint16_t status_code) {
    mp_odata_response_t *response = (mp_odata_response_t *)malloc(sizeof(mp_odata_response_t));
    if (!response) {
        return NULL;
    }
    
    response->data = data;
    response->content_type = content_type ? strdup(content_type) : strdup("application/json");
    response->status_code = status_code;
    response->error = NULL;
    
    return response;
}

void mp_odata_response_free(mp_odata_response_t *response) {
    if (!response) {
        return;
    }
    
    if (response->data) {
        mp_json_free(response->data);
    }
    if (response->content_type) {
        free((void *)response->content_type);
    }
    if (response->error) {
        mp_odata_response_free_error(response->error);
    }
    
    free(response);
}

void mp_odata_response_free_error(mp_odata_error_t *error) {
    if (!error) {
        return;
    }
    
    if (error->code) {
        free((void *)error->code);
    }
    if (error->message) {
        free((void *)error->message);
    }
    if (error->target) {
        free((void *)error->target);
    }
    if (error->details) {
        free((void *)error->details);
    }
    if (error->inner_error) {
        mp_json_free(error->inner_error);
    }
    
    free(error);
}

mp_odata_response_t *mp_odata_response_create_error(const char *code, const char *message, const char *target, const char *details, uint16_t status_code) {
    mp_odata_response_t *response = (mp_odata_response_t *)malloc(sizeof(mp_odata_response_t));
    if (!response) {
        return NULL;
    }
    
    response->data = NULL;
    response->content_type = strdup("application/json");
    response->status_code = status_code;
    
    mp_odata_error_t *error = (mp_odata_error_t *)malloc(sizeof(mp_odata_error_t));
    if (!error) {
        free(response);
        return NULL;
    }
    
    error->code = code ? strdup(code) : strdup("500");
    error->message = message ? strdup(message) : strdup("Internal Server Error");
    error->target = target ? strdup(target) : NULL;
    error->details = details ? strdup(details) : NULL;
    error->inner_error = NULL;
    
    response->error = error;
    
    // Create error response data
    mp_json_value_t *error_obj = mp_json_object();
    mp_json_object_add(error_obj, "error", mp_json_object());
    mp_json_value_t *error_details = mp_json_object_get(error_obj, "error");
    mp_json_object_add(error_details, "code", create_json_string(error->code));
    mp_json_object_add(error_details, "message", create_json_string(error->message));
    if (error->target) {
        mp_json_object_add(error_details, "target", create_json_string(error->target));
    }
    if (error->details) {
        mp_json_object_add(error_details, "details", create_json_string(error->details));
    }
    
    response->data = error_obj;
    
    return response;
}

mp_odata_response_t *mp_odata_generate_response(mp_odata_service_t *service, const char *entity_set_name, mp_json_value_t **entities, size_t count, mp_odata_query_options_t *options, mp_odata_context_t *context) {
    if (!service || !entity_set_name) {
        return NULL;
    }
    
    // Find entity set
    mp_odata_entity_set_t *entity_set = service->metadata.entity_sets;
    while (entity_set) {
        if (strcmp(entity_set->name, entity_set_name) == 0) {
            break;
        }
        entity_set = entity_set->next;
    }
    
    if (!entity_set) {
        return mp_odata_response_create_error("404", "Entity set not found", entity_set_name, NULL, 404);
    }
    
    // Apply query options
    mp_json_value_t **filtered = entities;
    size_t filtered_count = count;
    
    // Apply $filter
    if (options && options->filter) {
        mp_json_value_t **temp = NULL;
        size_t temp_count = 0;
        if (mp_odata_apply_filter(filtered, filtered_count, options->filter, &temp, &temp_count) == 0) {
            filtered = temp;
            filtered_count = temp_count;
        }
    }
    
    // Apply $orderby
    if (options && options->orderby_count > 0) {
        mp_odata_apply_orderby(filtered, filtered_count, options->orderby, options->orderby_count);
    }
    
    // Apply $top and $skip
    if (options && (options->top_set || options->skip_set)) {
        mp_json_value_t **temp = NULL;
        size_t temp_count = 0;
        if (mp_odata_apply_paging(filtered, filtered_count, options->top, options->skip, &temp, &temp_count) == 0) {
            if (filtered != entities) {
                free(filtered);
            }
            filtered = temp;
            filtered_count = temp_count;
        }
    }
    
    // Create response
    mp_json_value_t *response_data = mp_odata_wrap_collection_with_context(
        filtered, filtered_count, context, entity_set_name, (int64_t)count);
    
    if (filtered != entities) {
        free(filtered);
    }
    
    if (!response_data) {
        return mp_odata_response_create_error("500", "Failed to create response", NULL, NULL, 500);
    }
    
    return mp_odata_response_create(response_data, "application/json", 200);
}

mp_json_value_t *mp_odata_wrap_entity_with_context(mp_json_value_t *entity, mp_odata_context_t *context, const char *entity_set_name) {
    if (!entity) {
        return NULL;
    }
    
    mp_json_value_t *result = mp_json_object();
    if (!result) {
        return NULL;
    }
    
    // Add @odata.context
    mp_json_value_t *ctx = mp_odata_generate_context(context, entity_set_name);
    if (ctx) {
        mp_json_object_add(result, "@odata.context", ctx);
    }
    
    // Copy all properties from entity
    if (mp_json_is_object(entity)) {
        mp_json_object_t *obj = &entity->data.object;
        for (size_t i = 0; i < obj->count; i++) {
            mp_json_value_t *key = obj->keys[i];
            mp_json_value_t *value = obj->values[i];
            
            // Skip @odata.* properties
            const char *key_str = mp_json_get_string(key);
            if (key_str && strncmp(key_str, "@odata.", 7) != 0) {
                mp_json_object_add(result, key_str, mp_json_deep_copy(value));
            }
        }
    }
    
    return result;
}

mp_json_value_t *mp_odata_wrap_collection_with_context(mp_json_value_t **entities, size_t count, mp_odata_context_t *context, const char *entity_set_name, int64_t total_count) {
    if (!entities) {
        return NULL;
    }
    
    mp_json_value_t *result = mp_json_object();
    if (!result) {
        return NULL;
    }
    
    // Add @odata.context
    mp_json_value_t *ctx = mp_odata_generate_context(context, entity_set_name);
    if (ctx) {
        mp_json_object_add(result, "@odata.context", ctx);
    }
    
    // Add @odata.count if requested
    if (total_count >= 0) {
        mp_json_object_add(result, "@odata.count", create_json_integer(total_count));
    }
    
    // Add value array
    mp_json_value_t *value = mp_json_array();
    for (size_t i = 0; i < count; i++) {
        mp_json_array_add(value, mp_odata_wrap_entity_with_context(entities[i], context, entity_set_name));
    }
    
    mp_json_object_add(result, "value", value);
    
    return result;
}

const char *mp_odata_get_type_name(mp_odata_type_t type) {
    switch (type) {
        case MP_ODATA_TYPE_STRING: return MP_ODATA_TYPE_STRING_NAME;
        case MP_ODATA_TYPE_INT32: return MP_ODATA_TYPE_INT32_NAME;
        case MP_ODATA_TYPE_INT64: return MP_ODATA_TYPE_INT64_NAME;
        case MP_ODATA_TYPE_DOUBLE: return MP_ODATA_TYPE_DOUBLE_NAME;
        case MP_ODATA_TYPE_BOOLEAN: return MP_ODATA_TYPE_BOOLEAN_NAME;
        case MP_ODATA_TYPE_DATETIME_OFFSET: return MP_ODATA_TYPE_DATETIME_OFFSET_NAME;
        case MP_ODATA_TYPE_DATE: return MP_ODATA_TYPE_DATE_NAME;
        case MP_ODATA_TYPE_TIME_OF_DAY: return MP_ODATA_TYPE_TIME_OF_DAY_NAME;
        case MP_ODATA_TYPE_DURATION: return MP_ODATA_TYPE_DURATION_NAME;
        case MP_ODATA_TYPE_GUID: return MP_ODATA_TYPE_GUID_NAME;
        case MP_ODATA_TYPE_BINARY: return MP_ODATA_TYPE_BINARY_NAME;
        case MP_ODATA_TYPE_COMPLEX: return "Edm.Complex";
        case MP_ODATA_TYPE_ENTITY: return "Edm.Entity";
        case MP_ODATA_TYPE_COLLECTION: return "Edm.Collection";
        case MP_ODATA_TYPE_ENUM: return "Edm.Enum";
        case MP_ODATA_TYPE_NULL: return "Edm.Null";
        default: return "Edm.String";
    }
}

mp_odata_type_t mp_odata_get_type_from_json(mp_json_value_t *value) {
    if (!value) {
        return MP_ODATA_TYPE_NULL;
    }
    
    switch (value->type) {
        case MP_JSON_NULL: return MP_ODATA_TYPE_NULL;
        case MP_JSON_BOOLEAN: return MP_ODATA_TYPE_BOOLEAN;
        case MP_JSON_NUMBER:
            if (value->data.number.is_integer) {
                if (value->data.number.integer_value >= INT32_MIN && value->data.number.integer_value <= INT32_MAX) {
                    return MP_ODATA_TYPE_INT32;
                } else {
                    return MP_ODATA_TYPE_INT64;
                }
            } else {
                return MP_ODATA_TYPE_DOUBLE;
            }
        case MP_JSON_STRING: return MP_ODATA_TYPE_STRING;
        case MP_JSON_OBJECT: return MP_ODATA_TYPE_COMPLEX;
        case MP_JSON_ARRAY: return MP_ODATA_TYPE_COLLECTION;
        default: return MP_ODATA_TYPE_STRING;
    }
}

mp_json_value_t *mp_odata_create_entity(const char *entity_set_name, const char *key) {
    mp_json_value_t *entity = mp_json_object();
    if (!entity) {
        return NULL;
    }
    
    // Add @odata.id
    char id[256];
    snprintf(id, sizeof(id), "%s(%s)", entity_set_name, key ? key : "");
    mp_json_object_add(entity, "@odata.id", create_json_string(id));
    
    // Add @odata.etag (optional)
    // mp_json_object_add(entity, "@odata.etag", create_json_string("W/\"123456789\""));
    
    // Add @odata.editLink (optional)
    // mp_json_object_add(entity, "@odata.editLink", create_json_string(entity_set_name));
    
    return entity;
}

void mp_odata_entity_add_property(mp_json_value_t *entity, const char *name, mp_json_value_t *value) {
    if (!entity || !name || !value) {
        return;
    }
    
    mp_json_object_add(entity, name, value);
}
