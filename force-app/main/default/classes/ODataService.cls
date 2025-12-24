@RestResource(urlMapping='/odata/v4/*')
global with sharing class ODataService {
    // Supported OData system query options
    private static final Set<String> SUPPORTED_QUERIES = new Set<String>{'$select', '$filter', '$top', '$skip', '$metadata'};
    private static final Integer MAX_TOP = 100; // Governor limit safety
    private static final String ODATA_VERSION = '4.0';

    @HttpGet
    global static void doGet() {
        RestRequest req = RestContext.request;
        RestResponse res = RestContext.response;
        res.addHeader('Content-Type', 'application/json');
        res.addHeader('OData-Version', ODATA_VERSION);

        try {
            String requestPath = req.requestURI.substringAfter('/odata/v4/').toLowerCase();
            Map<String, String> queryParams = req.params;

            // Handle $metadata request
            if (requestPath == '$metadata') {
                handleMetadataRequest(res);
                return;
            }

            // Extract entity set (object name) from path, e.g., /odata/v4/Accounts
            String entitySet = requestPath.split('/')[0];
            if (String.isBlank(entitySet)) {
                throw new ODataException(400, 'Invalid entity set');
            }

            // Validate object accessibility
            Schema.SObjectType sObjectType = Schema.getGlobalDescribe().get(entitySet);
            if (sObjectType == null || !sObjectType.getDescribe().isAccessible()) {
                throw new ODataException(403, 'Object ' + entitySet + ' is not accessible');
            }

            // Parse query parameters
            String selectFields = queryParams.get('$select') != null ? queryParams.get('$select') : 'Id';
            String filter = queryParams.get('$filter');
            Integer top = queryParams.get('$top') != null ? Integer.valueOf(queryParams.get('$top')) : 50;
            Integer skip = queryParams.get('$skip') != null ? Integer.valueOf(queryParams.get('$skip')) : 0;

            // Validate parameters
            if (top > MAX_TOP || top < 1) {
                throw new ODataException(400, '$top must be between 1 and ' + MAX_TOP);
            }
            if (skip < 0) {
                throw new ODataException(400, '$skip must be non-negative');
            }

            // Build SOQL query
            String soqlQuery = buildSoqlQuery(sObjectType, selectFields, filter, top, skip);
            List<SObject> records = Database.query(soqlQuery);

            // Format OData response
            Map<String, Object> odataResponse = new Map<String, Object>{
                '@odata.context' => req.requestURI + '/$metadata#' + entitySet,
                'value' => records
            };
            res.statusCode = 200;
            res.responseBody = Blob.valueOf(JSON.serialize(odataResponse));
        } catch (ODataException ex) {
            res.statusCode = ex.statusCode;
            res.responseBody = Blob.valueOf(JSON.serialize(new ErrorResponse(ex.getMessage())));
        } catch (Exception ex) {
            res.statusCode = 500;
            res.responseBody = Blob.valueOf(JSON.serialize(new ErrorResponse('Internal server error: ' + ex.getMessage())));
        }
    }

    // Build SOQL query with FLS enforcement
    private static String buildSoqlQuery(Schema.SObjectType sObjectType, String selectFields, String filter, Integer top, Integer skip) {
        Schema.DescribeSObjectResult describe = sObjectType.getDescribe();
        String objectName = describe.getName();

        // Validate and sanitize select fields
        List<String> fields = selectFields.split(',');
        List<String> safeFields = new List<String>();
        Map<String, Schema.SObjectField> fieldMap = describe.fields.getMap();
        for (String field : fields) {
            field = field.trim().toLowerCase();
            if (fieldMap.containsKey(field) && fieldMap.get(field).getDescribe().isAccessible()) {
                safeFields.add(field);
            }
        }
        if (safeFields.isEmpty()) {
            throw new ODataException(400, 'No accessible fields specified');
        }

        // Build SELECT clause
        String soql = 'SELECT ' + String.join(safeFields, ', ') + ' FROM ' + objectName;

        // Handle $filter (basic eq operator only for simplicity)
        if (String.isNotBlank(filter)) {
            String sanitizedFilter = sanitizeFilter(filter);
            soql += ' WHERE ' + sanitizedFilter;
        }

        // Add LIMIT and OFFSET
        soql += ' LIMIT ' + top;
        if (skip > 0) {
            soql += ' OFFSET ' + skip;
        }

        return soql;
    }

    // Sanitize $filter to prevent SOQL injection (basic example)
    private static String sanitizeFilter(String filter) {
        if (String.isBlank(filter)) {
            return '';
        }
        // Example: Handle "Name eq 'Acme'" (simplified)
        String[] parts = filter.split(' eq ');
        if (parts.size() != 2) {
            throw new ODataException(400, 'Unsupported $filter syntax');
        }
        String field = parts[0].trim().toLowerCase();
        String value = parts[1].trim().replace('\'', '');

        // Validate field
        if (!Pattern.matches('[a-zA-Z0-9_]+', field)) {
            throw new ODataException(400, 'Invalid field name in $filter');
        }
        // Escape single quotes in value
        value = String.escapeSingleQuotes(value);
        return field + ' = \'' + value + '\'';
    }

    // Handle $metadata request
    private static void handleMetadataRequest(RestResponse res) {
        // Simplified EDMX metadata for OData v4
        String metadata = '<?xml version="1.0" encoding="utf-8"?>' +
            '<edmx:Edmx Version="4.0" xmlns:edmx="http://docs.oasis-open.org/odata/ns/edmx">' +
            '<edmx:DataServices>' +
            '<Schema Namespace="Salesforce" xmlns="http://docs.oasis-open.org/odata/ns/edm">';

        // Dynamically include accessible objects
        for (Schema.SObjectType sObjectType : Schema.getGlobalDescribe().values()) {
            Schema.DescribeSObjectResult describe = sObjectType.getDescribe();
            if (describe.isAccessible() && !describe.isCustomSetting()) {
                metadata += '<EntityType Name="' + describe.getName() + '">';
                metadata += '<Key><PropertyRef Name="Id"/></Key>';
                for (Schema.SObjectField field : describe.fields.getMap().values()) {
                    Schema.DescribeFieldResult fieldDescribe = field.getDescribe();
                    if (fieldDescribe.isAccessible()) {
                        String edmType = mapToEdmType(fieldDescribe.getType());
                        metadata += '<Property Name="' + fieldDescribe.getName() + '" Type="' + edmType + '" />';
                    }
                }
                metadata += '</EntityType>';
            }
        }

        metadata += '</Schema></edmx:DataServices></edmx:Edmx>';

        res.addHeader('Content-Type', 'application/xml');
        res.statusCode = 200;
        res.responseBody = Blob.valueOf(metadata);
    }

    // Map Salesforce field types to OData EDM types
    private static String mapToEdmType(Schema.DisplayType fieldType) {
        switch on fieldType {
            when STRING, TEXTAREA, URL, EMAIL, PHONE {
                return 'Edm.String';
            }
            when INTEGER {
                return 'Edm.Int32';
            }
            when DOUBLE, CURRENCY, PERCENT {
                return 'Edm.Decimal';
            }
            when BOOLEAN {
                return 'Edm.Boolean';
            }
            when DATE {
                return 'Edm.Date';
            }
            when DATETIME {
                return 'Edm.DateTimeOffset';
            }
            when else {
                return 'Edm.String';
            }
        }
    }

    // Custom exception class
    private class ODataException extends Exception {
        Integer statusCode;
        ODataException(Integer statusCode, String message) {
            this.statusCode = statusCode;
            this.setMessage(message);
        }
    }

    // Error response wrapper
    private class ErrorResponse {
        String error;
        ErrorResponse(String message) {
            this.error = message;
        }
    }
}