public without sharing class EmailAgentforceHandler implements Messaging.InboundEmailHandler {
    public Messaging.InboundEmailResult handleInboundEmail(Messaging.InboundEmail email, Messaging.InboundEnvelope envelope) {
        Messaging.InboundEmailResult result = new Messaging.InboundEmailResult();
      
    
        // if the TO is directly to the email_agentforce_handler@1msig93swwpux3surydsvylxjf5e7ao4hzc2qn8roy9ue6qk7i.gl-i4tuua0.can98.apex.salesforce.com
        //  then skip this
        if (email.toAddresses[0] != 'email_agentforce_handler@1msig93swwpux3surydsvylxjf5e7ao4hzc2qn8roy9ue6qk7i.gl-i4tuua0.can98.apex.salesforce.com') {
            createTaskWithEmailHistoryStripped(email);
        } else {

            letAgentHandleThis(email);

          
            // log the request
            // don't exceed Message__C limit of 131072

            Log__c log = new Log__c();
            log.Message__c = email.plainTextBody.left(131072);
            insert log;

        }

        
        

          
        result.success = true;
        return result;
        
    }

    public static void letAgentHandleThis(Messaging.InboundEmail email) {

        // Get the text of the emamil
        String emailBody = email.plainTextBody;
        
        Invocable.Action action = Invocable.Action.createCustomAction('generateAiAgentResponse', 'Copilot_for_Salesforce');
          
        action.setInvocationParameter('userMessage', emailBody);

        // Execute the action
        List<Invocable.Action.Result> actionResults = action.invoke();
        Invocable.Action.Result actionResult = actionResults[0];

        // Handle the response
        if (actionResult.isSuccess()) {

            // log the response to the Log__c object using the Message__C field.
            Log__c log = new Log__c();
            log.Message__C = (string) actionResult.getOutputParameters().get('agentResponse');
            insert log;

        } else {
            // log the response to the Log__c object using the Message__C field.
            Log__c log = new Log__c();
            log.Message__C = (string) actionResult.getOutputParameters().get('agentResponse');
            insert log;
}
    }

    public static void createTaskWithEmailHistoryStripped(Messaging.InboundEmail email) {
        // Extract the 'To' email address
        String toAddress = email.toAddresses != null && !email.toAddresses.isEmpty() ? email.toAddresses[0] : null;
        
        if (toAddress != null) {
            // Try to find a matching contact
            Contact contactRecord;
            try {
                contactRecord = [SELECT Id, Email FROM Contact WHERE Email = :toAddress LIMIT 1];
            } catch (QueryException e) {
                // Contact not found, create a new one
                contactRecord = new Contact();
                contactRecord.Email = toAddress;
                contactRecord.LastName = toAddress.substringBefore('@');
                insert contactRecord;
            }
            
            // Strip email history/replies from the body
            String strippedBody = stripEmailHistory(email.plainTextBody);
            
            // Create a Task of type Email
            Task emailTask = new Task();
            emailTask.Subject = 'Email: ' + email.subject;
            emailTask.Description = strippedBody;
            emailTask.Status = 'Completed';
            emailTask.Priority = 'Normal';
            emailTask.Type = 'Email';
            emailTask.WhoId = contactRecord.Id;
            emailTask.ActivityDate = Date.today();
            
            // Check for existing tasks with same contact, subject, and day
            List<Task> existingTasks = [SELECT Id FROM Task 
                WHERE WhoId = :contactRecord.Id 
                AND Subject = :emailTask.Subject 
                AND Type = 'Email'
                AND ActivityDate = TODAY 
                LIMIT 1];
            
            if (!existingTasks.isEmpty()) {
                emailTask.Is_Same_Day_Email__c = true;
            }
            
            insert emailTask;
        }
    }
    
    // Helper method to strip email history/replies
    private static String stripEmailHistory(String emailBody) {
        if (String.isBlank(emailBody)) {
            return '';
        }
        
        // Common reply separators to look for
        List<String> replySeparators = new List<String>{
            '-----Original Message-----',
            'From: ',
            'Sent: ',
            'To: ',
            'Cc: ',
            'Subject: ',
            'Reply-To: ',
            '-- ',
            '_____________________________'
        };
        
        String strippedText = emailBody;
        for (String separator : replySeparators) {
            Integer separatorIndex = strippedText.indexOf(separator);
            if (separatorIndex != -1) {
                strippedText = strippedText.substring(0, separatorIndex).trim();
                break; // Take the first match to avoid over-stripping
            }
        }
        
        // Additional cleanup of signature lines or other patterns if needed
        List<String> lines = strippedText.split('\n');
        List<String> cleanedLines = new List<String>();
        Boolean foundContent = false;
        
        for (String line : lines) {
            String trimmedLine = line.trim();
            if (trimmedLine.startsWith('--') || trimmedLine.startsWith('Thanks,') || trimmedLine.startsWith('Regards,')) {
                break; // Stop at signature
            }
            if (trimmedLine.startsWith('>')) {
                continue; // Skip lines that start with '>' indicating quoted text
            }
            // Check for pattern: starts with 'On' and ends with 'wrote:' and contains email
            if (trimmedLine.startsWith('On ') && trimmedLine.endsWith('wrote:') && trimmedLine.contains('<') && trimmedLine.contains('>') && trimmedLine.contains('@')) {
                continue; // Skip this line as it matches the reply pattern
            }
            if (!foundContent && String.isBlank(trimmedLine)) {
                continue; // Skip leading blank lines
            }
            foundContent = true;
            cleanedLines.add(line);
        }
        
        return String.join(cleanedLines, '\n').trim();
    }

}