public class DemoOrder {
    @AuraEnabled 
    @InvocableVariable(description='The ID of the order')
    public String id;

    @AuraEnabled 
    @InvocableVariable(description='The status of the order')
    public String status;

    @AuraEnabled 
    @InvocableVariable(description='The date of the order')
    public Date dateOfOrder;
    
}