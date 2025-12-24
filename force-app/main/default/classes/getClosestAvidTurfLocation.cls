public with sharing class getClosestAvidTurfLocation {
    public class Request {
        @InvocableVariable(required=true)
        public Double latitude;
        @InvocableVariable(required=true)
        public Double longitude;
    }
    public class Response {
        @InvocableVariable
        public Account account;
        @InvocableVariable
        public Double distanceInMiles;
    }

    @InvocableMethod(label='Get Closest Avid Turf Location' description='Gets the closest Avid Turf location to the given latitude and longitude')
    public static List<Response> getClosestAvidTurfLocation(List<Request> requests) {
        List<Response> responses = new List<Response>();
        // Fetch all Avid Turf Locations once to avoid SOQL in loop
        List<Account> turfLocations = [SELECT Id, Name, OwnerId, Geolocation__c, BillingAddress 
                                       FROM Account 
                                       WHERE Type = 'Avid Turf Location'];
        
        for (Request request : requests) {
            Account closestAccount = null;
            Double minDistance = null;
            Location requestLocation = Location.newInstance(request.latitude, request.longitude);
            
            // Manually calculate distance to each turf location
            for (Account turf : turfLocations) {
                if (turf.Geolocation__c != null) {
                    Location turfLocation = turf.Geolocation__c;
                    Double distance = Location.getDistance(requestLocation, turfLocation, 'mi');
                    if (minDistance == null || distance < minDistance) {
                        minDistance = distance;
                        closestAccount = turf;
                    }
                }
            }
            
            if (closestAccount != null) {
                Response response = new Response();
                response.account = closestAccount;
                response.distanceInMiles = minDistance;
                responses.add(response);
            }
        }
        return responses;
    }
}