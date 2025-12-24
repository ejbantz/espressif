public with sharing class UnusedReportsController {
    @AuraEnabled(cacheable=true)
    public static List<Report> getUnusedReports() {
        return [SELECT Id, Name, DeveloperName, FolderName FROM Report WHERE LastRunDate = NULL LIMIT 50];
    }
}