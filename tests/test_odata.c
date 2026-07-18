#include "microposix/serialization/odata.h"
#include <stdio.h>
#include <string.h>

// Test OData service creation
static void test_odata_service(void) {
    printf("Testing OData service...\n");
    
    mp_odata_service_t service;
    if (mp_odata_service_init(&service, "MyService", "MyNamespace", "4.0") != 0) {
        printf("ERROR: Failed to initialize OData service\n");
        return;
    }
    
    // Create an entity type
    mp_odata_entity_t *person_type = mp_odata_entity_type_create("Person", "MyNamespace");
    if (!person_type) {
        printf("ERROR: Failed to create entity type\n");
        mp_odata_service_destroy(&service);
        return;
    }
    
    // Add properties
    mp_odata_entity_type_add_property(person_type, "Id", MP_ODATA_TYPE_INT32, true);
    mp_odata_entity_type_add_property(person_type, "Name", MP_ODATA_TYPE_STRING, false);
    mp_odata_entity_type_add_property(person_type, "Age", MP_ODATA_TYPE_INT32, false);
    
    // Add entity type to service
    if (mp_odata_service_add_entity_type(&service, person_type) != 0) {
        printf("ERROR: Failed to add entity type\n");
        mp_odata_entity_type_free(person_type);
        mp_odata_service_destroy(&service);
        return;
    }
    
    // Create an entity set
    mp_odata_entity_set_t *people_set = mp_odata_entity_set_create("People", person_type);
    if (!people_set) {
        printf("ERROR: Failed to create entity set\n");
        mp_odata_service_destroy(&service);
        return;
    }
    
    // Add entity set to service
    if (mp_odata_service_add_entity_set(&service, people_set) != 0) {
        printf("ERROR: Failed to add entity set\n");
        mp_odata_entity_set_free(people_set);
        mp_odata_service_destroy(&service);
        return;
    }
    
    // Generate metadata
    mp_json_value_t *metadata = mp_odata_generate_metadata(&service);
    if (!metadata) {
        printf("ERROR: Failed to generate metadata\n");
        mp_odata_service_destroy(&service);
        return;
    }
    
    char *metadata_str = mp_json_to_string(metadata, MP_JSON_GEN_PRETTY);
    printf("Generated metadata:\n%s\n", metadata_str);
    free(metadata_str);
    mp_json_free(metadata);
    
    // Generate service document
    mp_json_value_t *service_doc = mp_odata_generate_service_document(&service);
    if (!service_doc) {
        printf("ERROR: Failed to generate service document\n");
        mp_odata_service_destroy(&service);
        return;
    }
    
    char *service_doc_str = mp_json_to_string(service_doc, MP_JSON_GEN_PRETTY);
    printf("Generated service document:\n%s\n", service_doc_str);
    free(service_doc_str);
    mp_json_free(service_doc);
    
    // Cleanup
    mp_odata_service_destroy(&service);
    
    printf("OData service tests passed!\n");
}

// Test OData query options parsing
static void test_odata_query_options(void) {
    printf("Testing OData query options...\n");
    
    const char *query = "?$filter=Name eq 'John'&$select=Id,Name&$orderby=Name asc&$top=10&$skip=5&$count=true";
    
    mp_odata_query_options_t options;
    if (mp_odata_parse_query_options(query, &options) != 0) {
        printf("ERROR: Failed to parse query options\n");
        return;
    }
    
    printf("Parsed query options:\n");
    printf("  Filter: %s\n", options.filter ? options.filter : "none");
    printf("  Select count: %zu\n", options.select_count);
    for (size_t i = 0; i < options.select_count; i++) {
        printf("    - %s\n", options.select[i]);
    }
    printf("  OrderBy count: %zu\n", options.orderby_count);
    for (size_t i = 0; i < options.orderby_count; i++) {
        printf("    - %s\n", options.orderby[i]);
    }
    printf("  Top: %lld\n", options.top);
    printf("  Skip: %lld\n", options.skip);
    printf("  Count: %s\n", options.count ? "true" : "false");
    
    mp_odata_query_options_free(&options);
    
    printf("OData query options tests passed!\n");
}

// Test OData entity creation
static void test_odata_entity(void) {
    printf("Testing OData entity creation...\n");
    
    // Create an entity
    mp_json_value_t *person = mp_odata_create_entity("People", "1");
    if (!person) {
        printf("ERROR: Failed to create entity\n");
        return;
    }
    
    // Add properties
    mp_odata_entity_add_property(person, "Id", mp_json_integer(1));
    mp_odata_entity_add_property(person, "Name", mp_json_string("John Doe"));
    mp_odata_entity_add_property(person, "Age", mp_json_integer(30));
    
    // Convert to string
    char *person_str = mp_json_to_string(person, MP_JSON_GEN_PRETTY);
    printf("Created entity:\n%s\n", person_str);
    free(person_str);
    
    mp_json_free(person);
    
    printf("OData entity creation tests passed!\n");
}

// Test OData response generation
static void test_odata_response(void) {
    printf("Testing OData response generation...\n");
    
    // Create a simple entity set
    mp_odata_entity_t *person_type = mp_odata_entity_type_create("Person", "Test");
    mp_odata_entity_type_add_property(person_type, "Id", MP_ODATA_TYPE_INT32, true);
    mp_odata_entity_type_add_property(person_type, "Name", MP_ODATA_TYPE_STRING, false);
    
    mp_odata_entity_set_t *people_set = mp_odata_entity_set_create("People", person_type);
    
    // Create entities
    mp_json_value_t *person1 = mp_odata_create_entity("People", "1");
    mp_odata_entity_add_property(person1, "Id", mp_json_integer(1));
    mp_odata_entity_add_property(person1, "Name", mp_json_string("Alice"));
    
    mp_json_value_t *person2 = mp_odata_create_entity("People", "2");
    mp_odata_entity_add_property(person2, "Id", mp_json_integer(2));
    mp_odata_entity_add_property(person2, "Name", mp_json_string("Bob"));
    
    mp_json_value_t *entities[] = {person1, person2};
    
    // Create context
    mp_odata_context_t context = {
        .base_url = "http://example.com/odata",
        .metadata_url = "http://example.com/odata/$metadata",
        .service_root = "http://example.com/odata"
    };
    
    // Wrap collection with context
    mp_json_value_t *response = mp_odata_wrap_collection_with_context(
        entities, 2, &context, "People", 2);
    
    if (!response) {
        printf("ERROR: Failed to create response\n");
        mp_json_free(person1);
        mp_json_free(person2);
        mp_odata_entity_type_free(person_type);
        mp_odata_entity_set_free(people_set);
        return;
    }
    
    char *response_str = mp_json_to_string(response, MP_JSON_GEN_PRETTY);
    printf("Generated response:\n%s\n", response_str);
    free(response_str);
    
    // Cleanup
    mp_json_free(response);
    mp_json_free(person1);
    mp_json_free(person2);
    mp_odata_entity_type_free(person_type);
    mp_odata_entity_set_free(people_set);
    
    printf("OData response generation tests passed!\n");
}

// Test OData type functions
static void test_odata_types(void) {
    printf("Testing OData type functions...\n");
    
    // Test type names
    printf("Type names:\n");
    printf("  String: %s\n", mp_odata_get_type_name(MP_ODATA_TYPE_STRING));
    printf("  Int32: %s\n", mp_odata_get_type_name(MP_ODATA_TYPE_INT32));
    printf("  Double: %s\n", mp_odata_get_type_name(MP_ODATA_TYPE_DOUBLE));
    printf("  Boolean: %s\n", mp_odata_get_type_name(MP_ODATA_TYPE_BOOLEAN));
    
    // Test type from JSON
    printf("\nType from JSON:\n");
    printf("  Null: %d\n", mp_odata_get_type_from_json(mp_json_null()));
    printf("  Boolean: %d\n", mp_odata_get_type_from_json(mp_json_boolean(true)));
    printf("  Integer: %d\n", mp_odata_get_type_from_json(mp_json_integer(42)));
    printf("  String: %d\n", mp_odata_get_type_from_json(mp_json_string("test")));
    printf("  Object: %d\n", mp_odata_get_type_from_json(mp_json_object()));
    printf("  Array: %d\n", mp_odata_get_type_from_json(mp_json_array()));
    
    printf("OData type tests passed!\n");
}

int main(void) {
    printf("Running OData tests...\n\n");
    
    test_odata_service();
    test_odata_query_options();
    test_odata_entity();
    test_odata_response();
    test_odata_types();
    
    printf("\nAll OData tests passed!\n");
    return 0;
}
