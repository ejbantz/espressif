import Foundation
import CoreBluetooth

class BLEManager: NSObject, ObservableObject {
    // UUIDs matching the ESP32
    let serviceUUID = CBUUID(string: "4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    let buttonCharUUID = CBUUID(string: "beb5483e-36e1-4688-b7f5-ea07361b26a8")
    let statusCharUUID = CBUUID(string: "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e")

    private var centralManager: CBCentralManager!
    private var esp32Peripheral: CBPeripheral?
    private var buttonCharacteristic: CBCharacteristic?
    private var statusCharacteristic: CBCharacteristic?

    @Published var isScanning = false
    @Published var isConnected = false
    @Published var buttonState = "Unknown"
    @Published var statusMessage = "Not connected"
    @Published var statusLog: [String] = []
    @Published var discoveredDevices: [CBPeripheral] = []

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
                peripheral.discoverCharacteristics([buttonCharUUID, statusCharUUID], for: service)
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
            }
        }
    }
}
