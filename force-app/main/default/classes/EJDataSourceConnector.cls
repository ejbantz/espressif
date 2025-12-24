global class EJDataSourceConnector extends DataSource.Connection {
    
    private DataSource.ConnectionParams connectionParams;

    global  EJDataSourceConnector(DataSource.ConnectionParams connectionParams) {
        this.connectionParams = connectionParams;
    }


    override global List<DataSource.Table> sync() {
        List<DataSource.Table> tables = new List<DataSource.Table>();
        
        
        
        List<DataSource.Column> columns = new List<DataSource.Column>();
        columns.add(DataSource.Column.text('Name', 255));
        columns.add(DataSource.Column.text('ExternalId', 255));
        columns.add(DataSource.Column.url('DisplayUrl'));
        columns.add(DataSource.Column.date('Date'));
        columns.add(DataSource.Column.number('HoursWorked', 10, 2));
        columns.add(DataSource.Column.text('ProjectName', 255));
        columns.add(DataSource.Column.text('WorkDescription', 1000));
        
        DataSource.Table timeEntryTable = new DataSource.Table();
        timeEntryTable.labelSingular = 'Time Entry';
        timeEntryTable.labelPlural = 'Time Entries';
        timeEntryTable.description = 'Time entries for a project';
        timeEntryTable.name = 'TimeEntry';
        timeEntryTable.nameColumn = 'Id';
        timeEntryTable.columns = columns;
        tables.add(timeEntryTable);



        return tables;
    }


    // query
    override global DataSource.TableResult query(DataSource.QueryContext queryContext) {
        List<Map<String, Object>> rows = new List<Map<String, Object>>();
        
        // Make an HTTP callout to another Salesforce org to get Time_Entry__c records
        Http http = new Http();
        HttpRequest request = new HttpRequest();
        // Placeholder for the endpoint URL of the connected Salesforce org
        // This should be configured based on DataSource.ConnectionParams
        string endpoint = 'https://dewwow.my.salesforce.com';
        request.setEndpoint( endpoint + '/services/data/v52.0/query?q=SELECT+Id,Name+FROM+Time_Entry__c');
        request.setMethod('GET');
        // Placeholder for authentication
        // This should use credentials or tokens from DataSource.ConnectionParams
        request.setHeader('Authorization', 'Bearer ' + this.connectionParams.oauthtoken);
        request.setHeader('Content-Type', 'application/json');
        
        try {
            HttpResponse response = http.send(request);
            if (response.getStatusCode() == 200) {
                Map<String, Object> jsonResponse = (Map<String, Object>) JSON.deserializeUntyped(response.getBody());
                List<Object> records = (List<Object>) jsonResponse.get('records');
                for (Object recordObj : records) {
                    Map<String, Object> record = (Map<String, Object>) recordObj;
                    Map<String, Object> row = new Map<String, Object>();
                    row.put('Name', record.get('Name'));
                    row.put('ExternalId', record.get('Id'));
                    row.put('DisplayUrl', record.get('Id'));
                   // row.put('Date', record.get('Date__c'));
                   // row.put('HoursWorked', record.get('Hours_Worked__c'));
                   // row.put('ProjectName', record.get('Project_Name__c'));
                   // row.put('WorkDescription', record.get('Work_Description__c'));
                    rows.add(row);
                }
            } else {
                System.debug('Error in HTTP callout: ' + response.getStatusCode() + ' ' + response.getStatus());
            }
        } catch (Exception e) {
            System.debug('Exception during HTTP callout: ' + e.getMessage());
        }
        
        return DataSource.TableResult.get(queryContext, rows);
    }


}