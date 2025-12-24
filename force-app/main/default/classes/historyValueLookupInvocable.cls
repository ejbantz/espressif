public with sharing class historyValueLookupInvocable {
    
    /**
     * Input class for the invocable method
     */
    public class HistoryLookupInput {
        @InvocableVariable(label='History Object API Name' description='The API name of the history object (e.g., AccountHistory, ContactHistory)' required=true)
        public String historyObjectName;
        
        @InvocableVariable(label='Field API Name' description='The API name of the field to look up in history' required=true)
        public String fieldApiName;
        
        @InvocableVariable(label='Record ID' description='The ID of the record to search history for' required=true)
        public String recordId;
        
        @InvocableVariable(label='Parent Field Name' description='The field name used to link to parent record (e.g., ParentId, AccountId, OpportunityId). Defaults to ParentId if not specified.' required=false)
        public String parentFieldName;
    }
    
    /**
     * Output class for the invocable method
     */
    public class HistoryLookupOutput {
        @InvocableVariable(label='Last Known String Value' description='The last known value as a string')
        public String lastKnownString;
        
        @InvocableVariable(label='Last Known DateTime Value' description='The last known value as a datetime (if applicable)')
        public DateTime lastKnownDateTime;
        
        @InvocableVariable(label='Success' description='Whether the lookup was successful')
        public Boolean success;
        
        @InvocableVariable(label='Error Message' description='Error message if lookup failed')
        public String errorMessage;
        
        @InvocableVariable(label='History Record Date' description='The date when this historical value was recorded')
        public DateTime historyRecordDate;
    }
    
    /**
     * Invocable method to find the last known value from field history
     */
    @InvocableMethod(label='Find Last Known Field Value' description='Finds the last known value for a field from Salesforce field history tracking')
    public static List<HistoryLookupOutput> findLastKnownValue(List<HistoryLookupInput> inputs) {
        List<HistoryLookupOutput> results = new List<HistoryLookupOutput>();
        
        // Pre-build all queries to avoid SOQL in loops
        Map<String, String> queryMap = new Map<String, String>();
        for (HistoryLookupInput input : inputs) {
            if (isValidInput(input)) {
                String parentField = String.isBlank(input.parentFieldName) ? 'ParentId' : input.parentFieldName;
                String queryKey = input.historyObjectName + '|' + input.fieldApiName + '|' + input.recordId + '|' + parentField;
                queryMap.put(queryKey, buildHistoryQuery(input.historyObjectName, input.fieldApiName, input.recordId, parentField));
            }
        }
        
        for (HistoryLookupInput input : inputs) {
            HistoryLookupOutput output = processHistoryLookup(input, queryMap);
            results.add(output);
        }
        
        return results;
    }
    
    /**
     * Process individual history lookup to reduce cognitive complexity
     */
    private static HistoryLookupOutput processHistoryLookup(HistoryLookupInput input, Map<String, String> queryMap) {
        HistoryLookupOutput output = new HistoryLookupOutput();
        output.success = false;
        
        try {
            if (!isValidInput(input)) {
                output.errorMessage = 'Missing required input parameters';
                return output;
            }
            
            String parentField = String.isBlank(input.parentFieldName) ? 'ParentId' : input.parentFieldName;
            String queryKey = input.historyObjectName + '|' + input.fieldApiName + '|' + input.recordId + '|' + parentField;
            String soqlQuery = queryMap.get(queryKey);
            
            if (soqlQuery != null) {
                List<SObject> historyRecords = Database.query(soqlQuery);
                processHistoryRecords(historyRecords, output);
            } else {
                output.errorMessage = 'Unable to build query for the given parameters';
            }
            
        } catch (Exception e) {
            output.errorMessage = 'Error occurred: ' + e.getMessage();
            System.debug('HistoryValueLookupInvocable Error: ' + e.getMessage());
            System.debug('Stack Trace: ' + e.getStackTraceString());
        }
        
        return output;
    }
    
    /**
     * Process history records and populate output
     */
    private static void processHistoryRecords(List<SObject> historyRecords, HistoryLookupOutput output) {
        if (historyRecords.isEmpty()) {
            output.errorMessage = 'No history records found for the specified field and record';
            return;
        }
        
        SObject latestRecord = historyRecords[0]; // Already ordered by CreatedDate DESC
        Object oldValue = latestRecord.get('OldValue');
        DateTime createdDate = (DateTime) latestRecord.get('CreatedDate');
        
        output.historyRecordDate = createdDate;
        output.success = true;
        
        if (oldValue != null) {
            output.lastKnownString = String.valueOf(oldValue);
        
        } else {
            output.lastKnownString = null;
            output.lastKnownDateTime = null;
        }
    }
    
     
    /**
     * Validate input parameters
     */
    private static Boolean isValidInput(HistoryLookupInput input) {
        return !String.isBlank(input.historyObjectName) && 
               !String.isBlank(input.fieldApiName) && 
               !String.isBlank(input.recordId);
    }
    
    /**
     * Builds the dynamic SOQL query for history lookup
     */
    private static String buildHistoryQuery(String historyObjectName, String fieldApiName, String recordId, String parentFieldName) {
        // Sanitize inputs to prevent SOQL injection
        historyObjectName = String.escapeSingleQuotes(historyObjectName);
        fieldApiName = String.escapeSingleQuotes(fieldApiName);
        recordId = String.escapeSingleQuotes(recordId);
        parentFieldName = String.escapeSingleQuotes(parentFieldName);
        
        // Build the query - we want records where the specified field changed
        String query = 'SELECT Id, CreatedDate, Field, OldValue, NewValue ' +
                      'FROM ' + historyObjectName + ' ' +
                      'WHERE Field = \'' + fieldApiName + '\' ' +
                      'AND ' + parentFieldName + ' = \'' + recordId + '\' ' +
                      'ORDER BY CreatedDate DESC ' +
                      'LIMIT 1';
        
        return query;
    }
    
   
}