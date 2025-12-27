import Foundation
import CoreBluetooth

struct WiFiNetwork: Identifiable, Codable {
    var id: String { ssid }
    let ssid: String
    let rssi: Int
    let open: Bool
    let saved: Bool
}

class BLEManager: NSObject, ObservableObject {
    // UUIDs matching the ESP32
    let serviceUUID = CBUUID(string: "4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    let buttonCharUUID = CBUUID(string: "beb5483e-36e1-4688-b7f5-ea07361b26a8")
    let statusCharUUID = CBUUID(string: "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e")
    let wifiScanCharUUID = CBUUID(string: "a1e8f5d2-7b3c-4e9a-8f2d-6c5b4a3d2e1f")
    let wifiCredCharUUID = CBUUID(string: "b2f9e6c3-8c4d-5f0b-9e3e-7d6c5b4a3f20")
    let wifiStatusCharUUID = CBUUID(string: "c3a0f7d4-9d5e-6f1c-0a4f-8e7d6c5b4a31")
    let sensorCharUUID = CBUUID(string: "d4b1e8f5-0e6f-7a2d-1b5a-9f8e7d6c5b42")
    let gpsCharUUID = CBUUID(string: "f6d3a9b7-2c4e-5f6a-8b9c-0d1e2f3a4b53")
    let cellCharUUID = CBUUID(string: "a7e4b0c8-3d5f-6a7b-9c0d-1e2f3a4b5c64")
    let salesforceCharUUID = CBUUID(string: "e5c2f8a6-1b3d-4e5f-9a7c-8d6b5e4f3a21")

    // Salesforce API config
    let sfEndpoint = "https://ejdev-dev-ed.develop.my.site.com/vforcesite/services/apexrest/sensor/reading"
    let sfApiKey = "LawnMonitor2024SecretKey"

    private var centralManager: CBCentralManager!
    private var esp32Peripheral: CBPeripheral?
    private var buttonCharacteristic: CBCharacteristic?
    private var statusCharacteristic: CBCharacteristic?
    private var wifiScanCharacteristic: CBCharacteristic?
    private var wifiCredCharacteristic: CBCharacteristic?
    private var wifiStatusCharacteristic: CBCharacteristic?
    private var sensorCharacteristic: CBCharacteristic?
    private var gpsCharacteristic: CBCharacteristic?
    private var cellCharacteristic: CBCharacteristic?
    private var salesforceCharacteristic: CBCharacteristic?

    @Published var isScanning = false
    @Published var isConnected = false
    @Published var buttonState = "Unknown"
    @Published var statusMessage = "Not connected"
    @Published var statusLog: [String] = []
    @Published var discoveredDevices: [CBPeripheral] = []
    @Published var wifiNetworks: [WiFiNetwork] = []
    @Published var isWifiScanning = false
    @Published var wifiConnectionStatus = "Unknown"
    @Published var sensorReading = "--"
    @Published var gpsStatus = "No modem"
    @Published var cellStatus = "No modem"

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    func startScanning() {
        guard centralManager.state == .poweredOn else {
            addLog("Bluetooth not ready")
            return
        }
        discoveredDevices = []
        isScanning = true
        centralManager.scanForPeripherals(withServices: [serviceUUID], options: nil)
        addLog("Scanning for ESP32-Sensor...")
    }

    func stopScanning() {
        centralManager.stopScan()
        isScanning = false
        addLog("Stopped scanning")
    }

    func connect(to peripheral: CBPeripheral) {
        stopScanning()
        esp32Peripheral = peripheral
        esp32Peripheral?.delegate = self
        centralManager.connect(peripheral, options: nil)
        addLog("Connecting to \(peripheral.name ?? "device")...")
    }

    func disconnect() {
        if let peripheral = esp32Peripheral {
            centralManager.cancelPeripheralConnection(peripheral)
        }
    }

    func scanWifiNetworks() {
        guard let characteristic = wifiScanCharacteristic,
              let peripheral = esp32Peripheral else {
            addLog("WiFi scan not available")
            return
        }
        isWifiScanning = true
        wifiNetworks = []
        let data = "SCAN".data(using: .utf8)!
        peripheral.writeValue(data, for: characteristic, type: .withResponse)
        addLog("Requested WiFi scan...")
    }

    func sendWifiCredentials(ssid: String, password: String) {
        guard let characteristic = wifiCredCharacteristic,
              let peripheral = esp32Peripheral else {
            addLog("Cannot send credentials")
            return
        }
        let credentials = "\(ssid):\(password)"
        let data = credentials.data(using: .utf8)!
        peripheral.writeValue(data, for: characteristic, type: .withResponse)
        addLog("Sent credentials for \(ssid)")
    }

    func forgetNetwork(ssid: String) {
        guard let characteristic = wifiCredCharacteristic,
              let peripheral = esp32Peripheral else {
            addLog("Cannot forget network")
            return
        }
        let command = "FORGET:\(ssid)"
        let data = command.data(using: .utf8)!
        peripheral.writeValue(data, for: characteristic, type: .withResponse)
        addLog("Forgetting \(ssid)")

        // Remove from local list
        wifiNetworks = wifiNetworks.map { network in
            if network.ssid == ssid {
                return WiFiNetwork(ssid: network.ssid, rssi: network.rssi, open: network.open, saved: false)
            }
            return network
        }
    }

    private func addLog(_ message: String) {
        let timestamp = DateFormatter.localizedString(from: Date(), dateStyle: .none, timeStyle: .medium)
        DispatchQueue.main.async {
            self.statusLog.insert("[\(timestamp)] \(message)", at: 0)
            if self.statusLog.count > 50 {
                self.statusLog.removeLast()
            }
        }
    }
}

// MARK: - CBCentralManagerDelegate
extension BLEManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            addLog("Bluetooth ready")
        case .poweredOff:
            addLog("Bluetooth is off")
        case .unauthorized:
            addLog("Bluetooth unauthorized")
        case .unsupported:
            addLog("Bluetooth not supported")
        default:
            addLog("Bluetooth state: \(central.state.rawValue)")
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        if !discoveredDevices.contains(where: { $0.identifier == peripheral.identifier }) {
            discoveredDevices.append(peripheral)
            addLog("Found: \(peripheral.name ?? "Unknown") (RSSI: \(RSSI))")
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        isConnected = true
        addLog("Connected to \(peripheral.name ?? "device")")
        peripheral.discoverServices([serviceUUID])
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        isConnected = false
        buttonState = "Unknown"
        statusMessage = "Disconnected"
        wifiNetworks = []
        wifiConnectionStatus = "Unknown"
        sensorReading = "--"
        gpsStatus = "No modem"
        cellStatus = "No modem"
        addLog("Disconnected")
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        addLog("Failed to connect: \(error?.localizedDescription ?? "unknown error")")
    }
}

// MARK: - CBPeripheralDelegate
extension BLEManager: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }

        for service in services {
            if service.uuid == serviceUUID {
                addLog("Found ESP32 service")
                peripheral.discoverCharacteristics([buttonCharUUID, statusCharUUID, wifiScanCharUUID, wifiCredCharUUID, wifiStatusCharUUID, sensorCharUUID, gpsCharUUID, cellCharUUID, salesforceCharUUID], for: service)
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }

        for characteristic in characteristics {
            if characteristic.uuid == buttonCharUUID {
                buttonCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                peripheral.readValue(for: characteristic)
                addLog("Subscribed to button state")
            }
            if characteristic.uuid == statusCharUUID {
                statusCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                peripheral.readValue(for: characteristic)
                addLog("Subscribed to status updates")
            }
            if characteristic.uuid == wifiScanCharUUID {
                wifiScanCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                addLog("WiFi config ready")
            }
            if characteristic.uuid == wifiCredCharUUID {
                wifiCredCharacteristic = characteristic
                addLog("WiFi credentials ready")
            }
            if characteristic.uuid == wifiStatusCharUUID {
                wifiStatusCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                peripheral.readValue(for: characteristic)
                addLog("WiFi status ready")
            }
            if characteristic.uuid == sensorCharUUID {
                sensorCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                peripheral.readValue(for: characteristic)
                addLog("Sensor ready")
            }
            if characteristic.uuid == gpsCharUUID {
                gpsCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                peripheral.readValue(for: characteristic)
                addLog("GPS ready")
            }
            if characteristic.uuid == cellCharUUID {
                cellCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                peripheral.readValue(for: characteristic)
                addLog("Cellular ready")
            }
            if characteristic.uuid == salesforceCharUUID {
                salesforceCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                addLog("Salesforce relay ready")
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard let data = characteristic.value,
              let value = String(data: data, encoding: .utf8) else { return }

        DispatchQueue.main.async {
            if characteristic.uuid == self.buttonCharUUID {
                self.buttonState = value
                self.addLog("Button: \(value)")
            } else if characteristic.uuid == self.statusCharUUID {
                self.statusMessage = value
                self.addLog("Status: \(value)")
            } else if characteristic.uuid == self.wifiScanCharUUID {
                self.isWifiScanning = false
                self.parseWifiNetworks(value)
            } else if characteristic.uuid == self.wifiStatusCharUUID {
                self.wifiConnectionStatus = value
            } else if characteristic.uuid == self.sensorCharUUID {
                self.sensorReading = value
            } else if characteristic.uuid == self.gpsCharUUID {
                self.gpsStatus = value
            } else if characteristic.uuid == self.cellCharUUID {
                self.cellStatus = value
            } else if characteristic.uuid == self.salesforceCharUUID {
                // Received data from ESP32 to post to Salesforce
                self.postToSalesforce(jsonPayload: value)
            }
        }
    }

    private func postToSalesforce(jsonPayload: String) {
        addLog("Posting to Salesforce...")

        guard let url = URL(string: sfEndpoint) else {
            addLog("Invalid SF URL")
            return
        }

        // Parse the JSON from ESP32 and add the API key
        guard let jsonData = jsonPayload.data(using: .utf8),
              var jsonDict = try? JSONSerialization.jsonObject(with: jsonData) as? [String: Any] else {
            addLog("Invalid JSON from ESP32")
            return
        }

        // Add the API key and connection type
        jsonDict["apiKey"] = sfApiKey
        jsonDict["connectionType"] = "Phone"

        // GPS coordinates come from ESP32 if available (latitude, longitude already in jsonDict)

        guard let finalData = try? JSONSerialization.data(withJSONObject: jsonDict) else {
            addLog("Failed to create JSON")
            return
        }

        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = finalData

        URLSession.shared.dataTask(with: request) { [weak self] data, response, error in
            DispatchQueue.main.async {
                if let error = error {
                    self?.addLog("SF Error: \(error.localizedDescription)")
                    return
                }

                if let httpResponse = response as? HTTPURLResponse {
                    if httpResponse.statusCode == 200 || httpResponse.statusCode == 201 {
                        self?.addLog("SF: Success!")
                    } else {
                        let responseStr = data.flatMap { String(data: $0, encoding: .utf8) } ?? "No response"
                        self?.addLog("SF Error \(httpResponse.statusCode): \(responseStr)")
                    }
                }
            }
        }.resume()
    }

    private func parseWifiNetworks(_ json: String) {
        guard let data = json.data(using: .utf8) else {
            addLog("Failed to parse WiFi data")
            return
        }

        do {
            let networks = try JSONDecoder().decode([WiFiNetwork].self, from: data)
            wifiNetworks = networks.sorted { $0.rssi > $1.rssi }
            addLog("Found \(networks.count) WiFi networks")
        } catch {
            addLog("JSON parse error: \(error.localizedDescription)")
        }
    }
}
