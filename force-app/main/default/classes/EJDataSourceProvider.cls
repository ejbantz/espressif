global class EJDataSourceProvider extends DataSource.Provider {
    // auth capability
    override global List<DataSource.AuthenticationCapability> getAuthenticationCapabilities() {
        List<DataSource.AuthenticationCapability> capabilities = new List<DataSource.AuthenticationCapability>();
        capabilities.add(DataSource.AuthenticationCapability.ANONYMOUS);
        capabilities.add(DataSource.AuthenticationCapability.BASIC);
        capabilities.add(DataSource.AuthenticationCapability.OAUTH);
        return capabilities;
    }

    // getCapabilities
    override global List<DataSource.Capability> getCapabilities() {
        List<DataSource.Capability> capabilities = new List<DataSource.Capability>();
        capabilities.add(DataSource.Capability.SEARCH);
        capabilities.add(DataSource.Capability.ROW_QUERY);
        return capabilities;
    }

    // getConnection
    override global DataSource.Connection getConnection(DataSource.ConnectionParams connectionParams) {
        return new EJDataSourceConnector(connectionParams);
    }

}