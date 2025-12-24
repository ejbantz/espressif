@IsTest
public class HistoryValueLookupInvocableTest {
    
    @TestSetup
    static void setupTestData() {
        // Create test account with history tracking enabled
        Account testAccount = new Account(
            Name = 'Test Account',
            Phone = '555-1234'
        );
        insert testAccount;
        
        // Update the account to create history records
        testAccount.Phone = '555-5678';
        update testAccount;
        
        testAccount.Phone = '555-9999';
        update testAccount;
    }
    
    @IsTest
    static void testProcessHistoryRecordsWithDifferentParentFields() {
        // Test that we can use different parent field names
        Account testAccount = [SELECT Id FROM Account WHERE Name = 'Test Account' LIMIT 1];
        
        // Test with default ParentId (parentFieldName left blank)
        historyValueLookupInvocable.HistoryLookupInput input1 = new historyValueLookupInvocable.HistoryLookupInput();
        input1.historyObjectName = 'AccountHistory';
        input1.fieldApiName = 'Phone';
        input1.recordId = testAccount.Id;
        // parentFieldName left blank to test default
        
        // Test with explicitly set ParentId
        historyValueLookupInvocable.HistoryLookupInput input2 = new historyValueLookupInvocable.HistoryLookupInput();
        input2.historyObjectName = 'AccountHistory';
        input2.fieldApiName = 'Phone';
        input2.recordId = testAccount.Id;
        input2.parentFieldName = 'AccountId';
        
        List<historyValueLookupInvocable.HistoryLookupInput> inputs = new List<historyValueLookupInvocable.HistoryLookupInput>{input1, input2};
        
        Test.startTest();
        List<historyValueLookupInvocable.HistoryLookupOutput> results = historyValueLookupInvocable.findLastKnownValue(inputs);
        Test.stopTest();
        
    }

    @IsTest
    static void testFindLastKnownValue_Success() {
        // Get test account
        Account testAccount = [SELECT Id FROM Account WHERE Name = 'Test Account' LIMIT 1];
        
        // Create input for the invocable method
        historyValueLookupInvocable.HistoryLookupInput input = new historyValueLookupInvocable.HistoryLookupInput();
        input.historyObjectName = 'AccountHistory';
        input.fieldApiName = 'Phone';
        input.recordId = testAccount.Id;
        input.parentFieldName = 'AccountId';
        
        List<historyValueLookupInvocable.HistoryLookupInput> inputs = new List<historyValueLookupInvocable.HistoryLookupInput>{input};
        
        Test.startTest();
        List<historyValueLookupInvocable.HistoryLookupOutput> results = historyValueLookupInvocable.findLastKnownValue(inputs);
        Test.stopTest();
        
    }
    
    @IsTest
    static void testFindLastKnownValue_NoHistoryFound() {
        // Get test account
        Account testAccount = [SELECT Id FROM Account WHERE Name = 'Test Account' LIMIT 1];
        
        // Create input for a field that doesn't have history
        historyValueLookupInvocable.HistoryLookupInput input = new historyValueLookupInvocable.HistoryLookupInput();
        input.historyObjectName = 'AccountHistory';
        input.fieldApiName = 'NonExistentField__c';
        input.recordId = testAccount.Id;
        input.parentFieldName = 'AccountId';
        
        List<historyValueLookupInvocable.HistoryLookupInput> inputs = new List<historyValueLookupInvocable.HistoryLookupInput>{input};
        
        Test.startTest();
        List<historyValueLookupInvocable.HistoryLookupOutput> results = historyValueLookupInvocable.findLastKnownValue(inputs);
        Test.stopTest();
       
    }
    
    @IsTest
    static void testFindLastKnownValue_MissingInputs() {
        // Create input with missing required fields
        historyValueLookupInvocable.HistoryLookupInput input = new historyValueLookupInvocable.HistoryLookupInput();
        input.historyObjectName = ''; // Empty
        input.fieldApiName = 'Phone';
        input.recordId = 'someId';
        input.parentFieldName = 'ParentId';
        
        List<historyValueLookupInvocable.HistoryLookupInput> inputs = new List<historyValueLookupInvocable.HistoryLookupInput>{input};
        
        Test.startTest();
        List<historyValueLookupInvocable.HistoryLookupOutput> results = historyValueLookupInvocable.findLastKnownValue(inputs);
        Test.stopTest();
        
    }
    
    @IsTest
    static void testFindLastKnownValue_InvalidObject() {
        // Get test account
        Account testAccount = [SELECT Id FROM Account WHERE Name = 'Test Account' LIMIT 1];
        
        // Create input with invalid object name
        historyValueLookupInvocable.HistoryLookupInput input = new historyValueLookupInvocable.HistoryLookupInput();
        input.historyObjectName = 'InvalidObjectHistory';
        input.fieldApiName = 'Phone';
        input.recordId = testAccount.Id;
        input.parentFieldName = 'ParentId';
        
        List<historyValueLookupInvocable.HistoryLookupInput> inputs = new List<historyValueLookupInvocable.HistoryLookupInput>{input};
        
        Test.startTest();
        List<historyValueLookupInvocable.HistoryLookupOutput> results = historyValueLookupInvocable.findLastKnownValue(inputs);
        Test.stopTest();
        
      
    }
    
    @IsTest
    static void testFindLastKnownValue_MultipleInputs() {
        // Get test account
        Account testAccount = [SELECT Id FROM Account WHERE Name = 'Test Account' LIMIT 1];
        
        // Create multiple inputs
        historyValueLookupInvocable.HistoryLookupInput input1 = new historyValueLookupInvocable.HistoryLookupInput();
        input1.historyObjectName = 'AccountHistory';
        input1.fieldApiName = 'Phone';
        input1.recordId = testAccount.Id;
        input1.parentFieldName = 'ParentId';
        
        historyValueLookupInvocable.HistoryLookupInput input2 = new historyValueLookupInvocable.HistoryLookupInput();
        input2.historyObjectName = 'AccountHistory';
        input2.fieldApiName = 'NonExistentField__c';
        input2.recordId = testAccount.Id;
        input2.parentFieldName = 'ParentId';
        
        List<historyValueLookupInvocable.HistoryLookupInput> inputs = new List<historyValueLookupInvocable.HistoryLookupInput>{input1, input2};
        
        Test.startTest();
        List<historyValueLookupInvocable.HistoryLookupOutput> results = historyValueLookupInvocable.findLastKnownValue(inputs);
        Test.stopTest();
        
    
    }
    
    @IsTest
    static void testIsDateTimeValue() {
        // Test the private helper method indirectly by checking datetime parsing
        Account testAccount = new Account(Name = 'Test Account');
        insert testAccount;
        
        // Update with a datetime field to create history
        testAccount.Name = 'Updated Account';
        update testAccount;
        
        historyValueLookupInvocable.HistoryLookupInput input = new historyValueLookupInvocable.HistoryLookupInput();
        input.historyObjectName = 'AccountHistory';
        input.fieldApiName = 'Name';
        input.recordId = testAccount.Id;
        input.parentFieldName = 'ParentId';
        
        List<historyValueLookupInvocable.HistoryLookupInput> inputs = new List<historyValueLookupInvocable.HistoryLookupInput>{input};
        
        Test.startTest();
        List<historyValueLookupInvocable.HistoryLookupOutput> results = historyValueLookupInvocable.findLastKnownValue(inputs);
        Test.stopTest();
        
    }
    
    @IsTest
    static void testCustomParentFieldName() {
        // Test using a different parent field name (simulating OpportunityHistory with AccountId)
        Account testAccount = [SELECT Id FROM Account WHERE Name = 'Test Account' LIMIT 1];
        
        historyValueLookupInvocable.HistoryLookupInput input = new historyValueLookupInvocable.HistoryLookupInput();
        input.historyObjectName = 'AccountHistory';
        input.fieldApiName = 'Phone';
        input.recordId = testAccount.Id;
        input.parentFieldName = 'AccountId'; // This would be 'AccountId' for OpportunityHistory in real usage
        
        List<historyValueLookupInvocable.HistoryLookupInput> inputs = new List<historyValueLookupInvocable.HistoryLookupInput>{input};
        
        Test.startTest();
        List<historyValueLookupInvocable.HistoryLookupOutput> results = historyValueLookupInvocable.findLastKnownValue(inputs);
        Test.stopTest();
      
    }
}