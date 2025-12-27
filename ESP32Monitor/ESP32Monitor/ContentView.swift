import SwiftUI
import UIKit

struct ContentView: View {
    @StateObject private var bleManager = BLEManager()
    @State private var selectedNetwork: WiFiNetwork?
    @State private var showPasswordSheet = false
    @State private var wifiPassword = ""
    @State private var showCopiedFeedback = false

    var body: some View {
        NavigationView {
            ScrollView {
            VStack(spacing: 20) {
                // Connection Status Card
                VStack(spacing: 12) {
                    HStack {
                        Circle()
                            .fill(bleManager.isConnected ? Color.green : Color.red)
                            .frame(width: 12, height: 12)
                        Text(bleManager.isConnected ? "Connected" : "Disconnected")
                            .font(.headline)
                        Spacer()
                    }

                    if !bleManager.isConnected {
                        if bleManager.isScanning {
                            HStack {
                                ProgressView()
                                    .padding(.trailing, 8)
                                Text("Scanning...")
                                Spacer()
                                Button("Stop") {
                                    bleManager.stopScanning()
                                }
                                .buttonStyle(.bordered)
                            }
                        } else {
                            Button(action: { bleManager.startScanning() }) {
                                Label("Scan for ESP32", systemImage: "antenna.radiowaves.left.and.right")
                                    .frame(maxWidth: .infinity)
                            }
                            .buttonStyle(.borderedProminent)
                        }

                        // Device List
                        if !bleManager.discoveredDevices.isEmpty {
                            ForEach(bleManager.discoveredDevices, id: \.identifier) { device in
                                Button(action: { bleManager.connect(to: device) }) {
                                    HStack {
                                        Image(systemName: "cpu")
                                        Text(device.name ?? "Unknown Device")
                                        Spacer()
                                        Image(systemName: "chevron.right")
                                    }
                                    .padding()
                                    .background(Color(.systemGray6))
                                    .cornerRadius(8)
                                }
                                .buttonStyle(.plain)
                            }
                        }
                    } else {
                        Button(action: { bleManager.disconnect() }) {
                            Label("Disconnect", systemImage: "xmark.circle")
                                .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.bordered)
                        .tint(.red)
                    }
                }
                .padding()
                .background(Color(.systemBackground))
                .cornerRadius(12)
                .shadow(radius: 2)

                // Device Info Card
                VStack(spacing: 12) {
                    // WiFi Status
                    HStack {
                        Image(systemName: bleManager.wifiConnectionStatus == "Disconnected" || bleManager.wifiConnectionStatus == "Unknown" ? "wifi.slash" : "wifi")
                            .font(.title2)
                            .foregroundColor(bleManager.wifiConnectionStatus == "Disconnected" || bleManager.wifiConnectionStatus == "Unknown" ? .red : .green)
                            .frame(width: 30)
                        VStack(alignment: .leading) {
                            Text("WiFi")
                                .font(.caption)
                                .foregroundColor(.secondary)
                            Text(bleManager.wifiConnectionStatus)
                                .font(.subheadline)
                                .fontWeight(.medium)
                        }
                        Spacer()
                    }
                    .padding(12)
                    .background(Color(.systemGray6))
                    .cornerRadius(8)

                    // Sensor Reading
                    HStack {
                        Image(systemName: "drop.fill")
                            .font(.title2)
                            .foregroundColor(.cyan)
                            .frame(width: 30)
                        VStack(alignment: .leading) {
                            Text("Temp | Moisture")
                                .font(.caption)
                                .foregroundColor(.secondary)
                            Text(bleManager.sensorReading)
                                .font(.title3)
                                .fontWeight(.semibold)
                        }
                        Spacer()
                    }
                    .padding(12)
                    .background(Color(.systemGray6))
                    .cornerRadius(8)

                    // Button State
                    HStack {
                        Image(systemName: bleManager.buttonState == "PRESSED" ? "button.programmable" : "button.programmable.square")
                            .font(.title2)
                            .foregroundColor(bleManager.buttonState == "PRESSED" ? .orange : .gray)
                            .frame(width: 30)
                        VStack(alignment: .leading) {
                            Text("Button")
                                .font(.caption)
                                .foregroundColor(.secondary)
                            Text(bleManager.buttonState)
                                .font(.subheadline)
                                .fontWeight(.medium)
                        }
                        Spacer()
                    }
                    .padding(12)
                    .background(
                        RoundedRectangle(cornerRadius: 8)
                            .fill(bleManager.buttonState == "PRESSED" ? Color.orange.opacity(0.2) : Color(.systemGray6))
                    )
                }
                .padding()
                .background(Color(.systemBackground))
                .cornerRadius(12)
                .shadow(radius: 2)

                // Status Card
                VStack(alignment: .leading, spacing: 8) {
                    Text("Latest Status")
                        .font(.caption)
                        .foregroundColor(.secondary)

                    Text(bleManager.statusMessage)
                        .font(.system(.body, design: .monospaced))
                        .padding()
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .background(Color(.systemGray6))
                        .cornerRadius(8)
                }
                .padding()
                .background(Color(.systemBackground))
                .cornerRadius(12)
                .shadow(radius: 2)

                // WiFi Configuration Card
                if bleManager.isConnected {
                    VStack(alignment: .leading, spacing: 12) {
                        HStack {
                            Text("WiFi Configuration")
                                .font(.headline)
                            Spacer()
                            if bleManager.isWifiScanning {
                                ProgressView()
                                    .scaleEffect(0.8)
                            }
                        }

                        Button(action: { bleManager.scanWifiNetworks() }) {
                            Label("Scan Networks", systemImage: "wifi")
                                .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.borderedProminent)
                        .disabled(bleManager.isWifiScanning)

                        if !bleManager.wifiNetworks.isEmpty {
                            ForEach(bleManager.wifiNetworks) { network in
                                HStack {
                                    Button(action: {
                                        if network.open {
                                            bleManager.sendWifiCredentials(ssid: network.ssid, password: "")
                                        } else {
                                            selectedNetwork = network
                                            wifiPassword = ""
                                            showPasswordSheet = true
                                        }
                                    }) {
                                        HStack {
                                            Image(systemName: wifiIcon(for: network.rssi))
                                                .foregroundColor(.blue)
                                            VStack(alignment: .leading) {
                                                Text(network.ssid)
                                                    .foregroundColor(.primary)
                                                HStack(spacing: 4) {
                                                    if network.open {
                                                        Text("Open")
                                                            .font(.caption2)
                                                            .foregroundColor(.green)
                                                    }
                                                    if network.saved {
                                                        Text("Saved")
                                                            .font(.caption2)
                                                            .foregroundColor(.orange)
                                                    }
                                                    Text("\(network.rssi) dBm")
                                                        .font(.caption2)
                                                        .foregroundColor(.secondary)
                                                }
                                            }
                                            Spacer()
                                            if !network.open {
                                                Image(systemName: "lock.fill")
                                                    .foregroundColor(.secondary)
                                                    .font(.caption)
                                            }
                                        }
                                    }
                                    .buttonStyle(.plain)

                                    if network.saved {
                                        Button(action: {
                                            bleManager.forgetNetwork(ssid: network.ssid)
                                        }) {
                                            Image(systemName: "xmark.circle.fill")
                                                .foregroundColor(.red)
                                        }
                                        .buttonStyle(.plain)
                                    }
                                }
                                .padding(10)
                                .background(Color(.systemGray6))
                                .cornerRadius(8)
                            }
                        }
                    }
                    .padding()
                    .background(Color(.systemBackground))
                    .cornerRadius(12)
                    .shadow(radius: 2)
                }

                // Log
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        Text("Activity Log")
                            .font(.caption)
                            .foregroundColor(.secondary)
                        Spacer()
                        Button(action: {
                            bleManager.statusLog.removeAll()
                        }) {
                            Label("Clear", systemImage: "trash")
                                .font(.caption)
                        }
                        .buttonStyle(.bordered)
                        .controlSize(.small)

                        Button(action: {
                            let logText = bleManager.statusLog.joined(separator: "\n")
                            UIPasteboard.general.string = logText
                            showCopiedFeedback = true
                            DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) {
                                showCopiedFeedback = false
                            }
                        }) {
                            Label(showCopiedFeedback ? "Copied!" : "Copy", systemImage: showCopiedFeedback ? "checkmark" : "doc.on.doc")
                                .font(.caption)
                        }
                        .buttonStyle(.bordered)
                        .controlSize(.small)
                        .tint(showCopiedFeedback ? .green : nil)
                    }

                    ScrollView {
                        LazyVStack(alignment: .leading, spacing: 4) {
                            ForEach(bleManager.statusLog, id: \.self) { log in
                                Text(log)
                                    .font(.system(.caption, design: .monospaced))
                                    .foregroundColor(.secondary)
                            }
                        }
                    }
                    .frame(maxHeight: 150)
                    .padding(8)
                    .background(Color(.systemGray6))
                    .cornerRadius(8)
                }
                .padding()
                .background(Color(.systemBackground))
                .cornerRadius(12)
                .shadow(radius: 2)

            }
            .padding()
            }
            .background(Color(.systemGroupedBackground))
            .navigationTitle("EJTEST Monitor")
        }
        .sheet(isPresented: $showPasswordSheet) {
            NavigationView {
                VStack(spacing: 20) {
                    Text("Enter password for")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                    Text(selectedNetwork?.ssid ?? "")
                        .font(.headline)

                    SecureField("Password", text: $wifiPassword)
                        .textFieldStyle(.roundedBorder)
                        .padding(.horizontal)

                    Button(action: {
                        if let network = selectedNetwork {
                            bleManager.sendWifiCredentials(ssid: network.ssid, password: wifiPassword)
                        }
                        showPasswordSheet = false
                    }) {
                        Text("Connect")
                            .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.borderedProminent)
                    .padding(.horizontal)
                    .disabled(wifiPassword.isEmpty)

                    Spacer()
                }
                .padding(.top, 30)
                .navigationTitle("WiFi Password")
                .navigationBarTitleDisplayMode(.inline)
                .toolbar {
                    ToolbarItem(placement: .cancellationAction) {
                        Button("Cancel") {
                            showPasswordSheet = false
                        }
                    }
                }
            }
            .presentationDetents([.medium])
        }
    }

    private func wifiIcon(for rssi: Int) -> String {
        switch rssi {
        case -50...0:
            return "wifi"
        case -70..<(-50):
            return "wifi"
        case -80..<(-70):
            return "wifi.exclamationmark"
        default:
            return "wifi.slash"
        }
    }
}

#Preview {
    ContentView()
}
