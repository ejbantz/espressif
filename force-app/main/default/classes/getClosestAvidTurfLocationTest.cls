@isTest
private class getClosestAvidTurfLocationTest {


@IsTest
    static void testGetClosestAvidTurfLocation() {
        // Create test data - Avid Turf Locations
        Account turfLocation1 = new Account(
            Name = 'Turf Location 1',
            Type = 'Avid Turf Location',
            Geolocation__Latitude__s = 37.7749,
            Geolocation__Longitude__s = -122.4194
        );
        Account turfLocation2 = new Account(
            Name = 'Turf Location 2',
            Type = 'Avid Turf Location',
            Geolocation__Latitude__s = 34.0522,
            Geolocation__Longitude__s = -118.2437
        );
        insert new List<Account>{ turfLocation1, turfLocation2 };

        // Create test data - Request
        getClosestAvidTurfLocation.Request request = new getClosestAvidTurfLocation.Request();
        request.latitude = 37.7743;
        request.longitude = -122.4192;

        // Call the method
        Test.startTest();
        List<getClosestAvidTurfLocation.Response> responses = getClosestAvidTurfLocation.getClosestAvidTurfLocation(new List<getClosestAvidTurfLocation.Request>{ request });
        Test.stopTest();

        // Verify the result
        System.assertEquals(1, responses.size(), 'Expected one response');
        getClosestAvidTurfLocation.Response response = responses[0];
        System.assertEquals(turfLocation1.Id, response.account.Id, 'Expected Turf Location 1 as the closest');
        System.assert(response.distanceInMiles < 1, 'Expected distance to be less than 1 mile');
    }

}