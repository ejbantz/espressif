@RestResource(urlMapping='/sensor/firmware')
global with sharing class FirmwareAPI {

    // API key for simple authentication
    private static final String API_KEY = 'LawnMonitor2024SecretKey';

    @HttpGet
    global static String getFirmwareInfo() {
        RestRequest req = RestContext.request;
        RestResponse res = RestContext.response;

        // Validate API key from query param or header
        String providedKey = req.params.get('apiKey');
        if (String.isBlank(providedKey)) {
            providedKey = req.headers.get('X-API-Key');
        }
        if (String.isBlank(providedKey) || providedKey != API_KEY) {
            res.statusCode = 401;
            return '{"success":false,"error":"Invalid or missing API key"}';
        }

        try {
            // Get device type from query param (optional, defaults to ESP32_Sensor)
            String deviceType = req.params.get('deviceType');
            if (String.isBlank(deviceType)) {
                deviceType = 'ESP32_Sensor';
            }

            // Query custom metadata for active firmware config
            List<Firmware_Config__mdt> configs = [
                SELECT DeveloperName, Firmware_Version__c, Resource_Name__c, Is_Active__c
                FROM Firmware_Config__mdt
                WHERE DeveloperName = :deviceType AND Is_Active__c = true
                LIMIT 1
            ];

            if (configs.isEmpty()) {
                res.statusCode = 404;
                return '{"success":false,"error":"No active firmware config found for device type: ' + deviceType + '"}';
            }

            Firmware_Config__mdt config = configs[0];

            // Build the static resource URL
            // Format: https://[domain]/[sitepath]/resource/[resourcename]
            String baseUrl = Site.getBaseUrl();
            if (String.isBlank(baseUrl)) {
                // Fallback for non-Site context
                baseUrl = URL.getOrgDomainUrl().toExternalForm();
            }
            String downloadUrl = baseUrl + '/resource/' + config.Resource_Name__c;

            res.statusCode = 200;
            return '{"success":true,' +
                   '"version":"' + config.Firmware_Version__c + '",' +
                   '"resourceName":"' + config.Resource_Name__c + '",' +
                   '"downloadUrl":"' + downloadUrl + '"}';

        } catch (Exception e) {
            res.statusCode = 500;
            return '{"success":false,"error":"' + e.getMessage().escapeJava() + '"}';
        }
    }
}
