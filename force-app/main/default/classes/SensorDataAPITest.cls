@isTest
private class SensorDataAPITest {

    private static final String VALID_API_KEY = 'LawnMonitor2024SecretKey';

    @isTest
    static void testCreateReadingSuccess() {
        RestRequest req = new RestRequest();
        RestResponse res = new RestResponse();

        req.requestURI = '/services/apexrest/sensor/reading';
        req.httpMethod = 'POST';
        req.headers.put('X-API-Key', VALID_API_KEY);
        req.requestBody = Blob.valueOf('{"temperature":25.5,"humidity":60.0,"deviceId":"ESP01-001"}');

        RestContext.request = req;
        RestContext.response = res;

        Test.startTest();
        String result = SensorDataAPI.createReading();
        Test.stopTest();

        System.assertEquals(201, res.statusCode, 'Should return 201 Created');
        System.assert(result.contains('"success":true'), 'Should return success');

        List<Sensor_Reading__c> readings = [SELECT Id, Temperature__c, Humidity__c, Device_Id__c FROM Sensor_Reading__c];
        System.assertEquals(1, readings.size(), 'Should create one reading');
        System.assertEquals(25.5, readings[0].Temperature__c, 'Temperature should match');
        System.assertEquals(60.0, readings[0].Humidity__c, 'Humidity should match');
        System.assertEquals('ESP01-001', readings[0].Device_Id__c, 'Device ID should match');
    }

    @isTest
    static void testCreateReadingInvalidApiKey() {
        RestRequest req = new RestRequest();
        RestResponse res = new RestResponse();

        req.requestURI = '/services/apexrest/sensor/reading';
        req.httpMethod = 'POST';
        req.headers.put('X-API-Key', 'wrong-key');
        req.requestBody = Blob.valueOf('{"temperature":25.5}');

        RestContext.request = req;
        RestContext.response = res;

        Test.startTest();
        String result = SensorDataAPI.createReading();
        Test.stopTest();

        System.assertEquals(401, res.statusCode, 'Should return 401 Unauthorized');
        System.assert(result.contains('Invalid or missing API key'), 'Should return auth error');
    }

    @isTest
    static void testCreateReadingMissingApiKey() {
        RestRequest req = new RestRequest();
        RestResponse res = new RestResponse();

        req.requestURI = '/services/apexrest/sensor/reading';
        req.httpMethod = 'POST';
        req.requestBody = Blob.valueOf('{"temperature":25.5}');

        RestContext.request = req;
        RestContext.response = res;

        Test.startTest();
        String result = SensorDataAPI.createReading();
        Test.stopTest();

        System.assertEquals(401, res.statusCode, 'Should return 401 Unauthorized');
    }

    @isTest
    static void testGetLatestReading() {
        // Create test data
        Sensor_Reading__c reading = new Sensor_Reading__c(
            Temperature__c = 22.0,
            Humidity__c = 55.0,
            Device_Id__c = 'ESP01-TEST',
            Reading_Timestamp__c = DateTime.now()
        );
        insert reading;

        RestRequest req = new RestRequest();
        RestResponse res = new RestResponse();

        req.requestURI = '/services/apexrest/sensor/reading';
        req.httpMethod = 'GET';
        req.headers.put('X-API-Key', VALID_API_KEY);
        req.params.put('deviceId', 'ESP01-TEST');

        RestContext.request = req;
        RestContext.response = res;

        Test.startTest();
        String result = SensorDataAPI.getLatestReading();
        Test.stopTest();

        System.assertEquals(200, res.statusCode, 'Should return 200 OK');
        System.assert(result.contains('"success":true'), 'Should return success');
        System.assert(result.contains('"temperature":22'), 'Should contain temperature');
    }

    @isTest
    static void testGetLatestReadingNotFound() {
        RestRequest req = new RestRequest();
        RestResponse res = new RestResponse();

        req.requestURI = '/services/apexrest/sensor/reading';
        req.httpMethod = 'GET';
        req.headers.put('X-API-Key', VALID_API_KEY);

        RestContext.request = req;
        RestContext.response = res;

        Test.startTest();
        String result = SensorDataAPI.getLatestReading();
        Test.stopTest();

        System.assertEquals(404, res.statusCode, 'Should return 404 Not Found');
    }
}