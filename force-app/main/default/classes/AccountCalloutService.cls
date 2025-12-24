/**
 * @description Service class to make callouts to create Account records using REST API with Named Credential
 */
public class AccountCalloutService {
    
    /**
     * @description Invocable method to create an Account via REST API using Named Credential
     * @param accountNames List of Account names to create
     * @return List of created Account record IDs
     */
    @InvocableMethod(label='Create Account via REST API' description='Creates an Account using REST API with Named Credential and returns the record ID')
    public static List<String> createAccount(List<String> accountNames) {
        List<String> createdAccountIds = new List<String>();
        
        if (accountNames == null || accountNames.isEmpty()) {
            return createdAccountIds;
        }
        
        // Use the first account name from the input list
        for (String accountName : accountNames) {
        
            try {
                // Prepare the Account data to send
                Map<String, Object> accountData = new Map<String, Object>();
                accountData.put('Name', accountName);
                
                // Serialize the data to JSON
                String jsonBody = JSON.serialize(accountData);
                
                // Set up the HTTP request
                HttpRequest req = new HttpRequest();
                req.setEndpoint('callout:DewWow_Production/services/data/v63.0/sobjects/Account');
                req.setMethod('POST');
                req.setHeader('Content-Type', 'application/json');
                req.setBody(jsonBody);
                
                // Make the callout
                Http http = new Http();
                HttpResponse res = http.send(req);
                
                // Check the response
                if (res.getStatusCode() == 201) {
                    // Success - extract the record ID from the response
                    Map<String, Object> responseMap = (Map<String, Object>) JSON.deserializeUntyped(res.getBody());
                    String recordId = (String) responseMap.get('id');
                    createdAccountIds.add(recordId);
                } else {
                    // Log error if creation failed
                    System.debug('Account creation failed. Status: ' + res.getStatusCode() + ', Body: ' + res.getBody());
                }
            } catch (Exception e) {
                // Log any exceptions during the callout
                System.debug('Error during callout: ' + e.getMessage());
            }
        }   
        
        return createdAccountIds;
    }
}