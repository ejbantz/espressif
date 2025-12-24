public with sharing class UpdatePicklistInvocable {
    public class UpdatePicklistInput {
        @InvocableVariable(required=true)
        public String objectName;
        @InvocableVariable(required=true)
        public String fieldName;
        @InvocableVariable(required=true)
        public String newPicklistValue;
    }

    @InvocableMethod(label='Update Picklist Value' description='Updates a picklist field on a record with a new value')
    public static List<String> updatePicklistValues(List<UpdatePicklistInput> inputs) {

        reestablishSession();
 


        List<String> results = new List<String>();
        for (UpdatePicklistInput input : inputs) {
            
            // Get the field Label and existing picklist values using describe.
            Schema.DescribeFieldResult fieldDescribe = Schema.getGlobalDescribe().get(input.objectName).getDescribe().fields.getMap().get(input.fieldName).getDescribe();
            string fieldLabel = fieldDescribe.getLabel();
            List<Schema.PicklistEntry> picklistEntries = fieldDescribe.getPicklistValues();



            MetadataService.MetadataPort service = new MetadataService.MetadataPort();
            service.endpoint_x = 'callout:ApexMDApi' + '/services/Soap/m/42.0';
            service.SessionHeader = new MetadataService.SessionHeader_element();
            service.SessionHeader.sessionId = '{!$Credential.OAuthToken}';


            MetadataService.CustomField customField = new MetadataService.CustomField();
            customField.fullName = input.objectName + '.' + input.fieldName;
            customField.label = fieldLabel;
            customField.type_x = 'Picklist';
    
            metadataservice.ValueSet valueSet = new metadataservice.ValueSet();
        
            metadataservice.ValueSetValuesDefinition vd = new metadataservice.ValueSetValuesDefinition();
            vd.sorted = false;
            vd.value = new List<MetadataService.CustomValue>();


            // Add each of the existing picklist values to the value set.
            for (Schema.PicklistEntry picklistEntry : picklistEntries) {
                metadataservice.CustomValue existingCustomValue = new metadataservice.CustomValue();
                existingCustomValue.fullName = picklistEntry.getValue();
                existingCustomValue.default_x = false;
                existingCustomValue.isActive = true;
                vd.value.add(existingCustomValue);
            }
            metadataservice.CustomValue newCustomValue = new metadataservice.CustomValue();
            newCustomValue.fullName = input.newPicklistValue;
            newCustomValue.default_x = false;
            newCustomValue.isActive = true;
            vd.value.add(newCustomValue);
    
            valueSet.valueSetDefinition = vd;
            customField.valueSet = valueSet;
    
            service.updateMetadata(new MetadataService.Metadata[] { customField });

            results.add('Finished');

        }
        return results;
    }
 // https://ejdev-dev-ed.develop.my.salesforce.com/services/oauth2/token
    public static void reestablishSession() {
        Http http = new Http();
        HttpRequest req = new HttpRequest();
        req.setEndpoint('callout:ApexMDApi/services/data/v62.0/limits/');
        req.setMethod('GET');
        HttpResponse httpResponse = http.send(req);
    }
}