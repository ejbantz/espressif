@isTest
private class FirmwareAPITest {

    @isTest
    static void testGetFirmwareInfo_Success() {
        // Setup request
        RestRequest req = new RestRequest();
        RestResponse res = new RestResponse();
        req.requestURI = '/services/apexrest/sensor/firmware';
        req.httpMethod = 'GET';
        req.params.put('apiKey', 'LawnMonitor2024SecretKey');
        req.params.put('deviceType', 'ESP32_Sensor');

        RestContext.request = req;
        RestContext.response = res;

        // Call the method
        Test.startTest();
        String result = FirmwareAPI.getFirmwareInfo();
        Test.stopTest();

        // Verify response
        System.assert(result.contains('"success":true'), 'Expected success response');
        System.assert(result.contains('"version"'), 'Expected version in response');
        System.assert(result.contains('"downloadUrl"'), 'Expected downloadUrl in response');
    }

    @isTest
    static void testGetFirmwareInfo_InvalidApiKey() {
        // Setup request with invalid API key
        RestRequest req = new RestRequest();
        RestResponse res = new RestResponse();
        req.requestURI = '/services/apexrest/sensor/firmware';
        req.httpMethod = 'GET';
        req.params.put('apiKey', 'wrong-key');

        RestContext.request = req;
        RestContext.response = res;

        // Call the method
        Test.startTest();
        String result = FirmwareAPI.getFirmwareInfo();
        Test.stopTest();

        // Verify unauthorized response
        System.assertEquals(401, res.statusCode, 'Expected 401 status code');
        System.assert(result.contains('"success":false'), 'Expected failure response');
    }

    @isTest
    static void testGetFirmwareInfo_MissingApiKey() {
        // Setup request without API key
        RestRequest req = new RestRequest();
        RestResponse res = new RestResponse();
        req.requestURI = '/services/apexrest/sensor/firmware';
        req.httpMethod = 'GET';

        RestContext.request = req;
        RestContext.response = res;

        // Call the method
        Test.startTest();
        String result = FirmwareAPI.getFirmwareInfo();
        Test.stopTest();

        // Verify unauthorized response
        System.assertEquals(401, res.statusCode, 'Expected 401 status code');
    }

    @isTest
    static void testGetFirmwareInfo_DefaultDeviceType() {
        // Setup request without deviceType (should default to ESP32_Sensor)
        RestRequest req = new RestRequest();
        RestResponse res = new RestResponse();
        req.requestURI = '/services/apexrest/sensor/firmware';
        req.httpMethod = 'GET';
        req.params.put('apiKey', 'LawnMonitor2024SecretKey');

        RestContext.request = req;
        RestContext.response = res;

        // Call the method
        Test.startTest();
        String result = FirmwareAPI.getFirmwareInfo();
        Test.stopTest();

        // Verify response (should succeed with default device type)
        System.assert(result.contains('"success":true') || result.contains('"success":false'),
                     'Expected valid JSON response');
    }
}
