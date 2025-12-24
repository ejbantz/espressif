public with sharing class agentforceOrderSearch {


    @InvocableMethod(label='Find Order' description='Searches for an order based on order number and email')
    public static List<Response> findOrder(List<Request> requests) {
        List<Response> responses = new List<Response>();
        for (Request req : requests) {
            Response res = new Response();

            Order order = new Order();
            // Logic to find order based on orderNumber and email will be implemented here
            order.id = '12345'; // Placeholder value
            order.status =  req.orderFilter.status; // Placeholder value
            order.dateOfOrder = Date.today(); // Placeholder value
            res.message = 'Order found';
            res.orders = new List<Order>();
            res.orders.add(order);

            responses.add(res);
        }
        return responses;
    }

    public class Request {

        @InvocableVariable(description='The email of the customer')
        public string email;

        @InvocableVariable(description='The order number to search for')
        public string orderNumber;

        @InvocableVariable(description='The order filter to apply to the search')
        public OrderFilter orderFilter;
    }

    public class OrderFilter {
        @InvocableVariable(description='The status.')
        public string status;

    }

    public class Response {
        @AuraEnabled 
        @InvocableVariable(description='An additional message to display to the user')
        public string message;

        @AuraEnabled 
        @InvocableVariable(description='The order object')
        public List<Order> orders;

    }

    public class Order {
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
}