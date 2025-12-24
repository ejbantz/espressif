@RestResource(urlMapping='/sensor/reading')
global with sharing class SensorDataAPI {

    // API key for simple authentication - store in Custom Metadata in production
    private static final String API_KEY = 'LawnMonitor2024SecretKey';

    @HttpPost
    global static String createReading() {
        RestRequest req = RestContext.request;
        RestResponse res = RestContext.response;

        try {
            // Parse JSON body
            Map<String, Object> body = (Map<String, Object>) JSON.deserializeUntyped(req.requestBody.toString());

            // Validate API key from body, query param, or header (Sites strip headers)
            String providedKey = body.containsKey('apiKey') ?
                String.valueOf(body.get('apiKey')) : req.params.get('apiKey');
            if (String.isBlank(providedKey)) {
                providedKey = req.headers.get('X-API-Key');
            }
            if (String.isBlank(providedKey) || providedKey != API_KEY) {
                res.statusCode = 401;
                return '{"success":false,"error":"Invalid or missing API key"}';
            }

            // Create sensor reading record
            Sensor_Reading__c reading = new Sensor_Reading__c();
            reading.Temperature__c = body.containsKey('temperature') ?
                Decimal.valueOf(String.valueOf(body.get('temperature'))) : null;
            reading.Humidity__c = body.containsKey('humidity') ?
                Decimal.valueOf(String.valueOf(body.get('humidity'))) : null;
            reading.Device_Id__c = body.containsKey('deviceId') ?
                String.valueOf(body.get('deviceId')) : 'unknown';
            reading.Function__c = body.containsKey('function') ?
                String.valueOf(body.get('function')) : null;
            reading.Reading_Timestamp__c = DateTime.now();

            insert reading;

            res.statusCode = 201;
            return '{"success":true,"id":"' + reading.Id + '","name":"' + reading.Name + '"}';

        } catch (Exception e) {
            res.statusCode = 400;
            return '{"success":false,"error":"' + e.getMessage().escapeJava() + '"}';
        }
    }

    @HttpGet
    global static String getLatestReading() {
        RestRequest req = RestContext.request;
        RestResponse res = RestContext.response;

        // Validate API key from query param or header (Sites strip headers)
        String providedKey = req.params.get('apiKey');
        if (String.isBlank(providedKey)) {
            providedKey = req.headers.get('X-API-Key');
        }
        if (String.isBlank(providedKey) || providedKey != API_KEY) {
            res.statusCode = 401;
            return '{"success":false,"error":"Invalid or missing API key"}';
        }

        try {
            String deviceId = req.params.get('deviceId');

            List<Sensor_Reading__c> readings;
            if (String.isNotBlank(deviceId)) {
                readings = [SELECT Id, Name, Temperature__c, Humidity__c, Device_Id__c, Reading_Timestamp__c
                           FROM Sensor_Reading__c
                           WHERE Device_Id__c = :deviceId
                           ORDER BY CreatedDate DESC LIMIT 1];
            } else {
                readings = [SELECT Id, Name, Temperature__c, Humidity__c, Device_Id__c, Reading_Timestamp__c
                           FROM Sensor_Reading__c
                           ORDER BY CreatedDate DESC LIMIT 1];
            }

            if (readings.isEmpty()) {
                res.statusCode = 404;
                return '{"success":false,"error":"No readings found"}';
            }

            Sensor_Reading__c r = readings[0];
            res.statusCode = 200;
            return '{"success":true,"reading":{"id":"' + r.Id + '","name":"' + r.Name +
                   '","temperature":' + r.Temperature__c + ',"humidity":' + r.Humidity__c +
                   ',"deviceId":"' + r.Device_Id__c + '","timestamp":"' + r.Reading_Timestamp__c + '"}}';

        } catch (Exception e) {
            res.statusCode = 500;
            return '{"success":false,"error":"' + e.getMessage().escapeJava() + '"}';
        }
    }
}