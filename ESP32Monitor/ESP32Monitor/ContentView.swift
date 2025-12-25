import SwiftUI

struct ContentView: View {
    @StateObject private var bleManager = BLEManager()

    var body: some View {
        NavigationView {
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

                // Button State Card
                VStack(spacing: 8) {
                    Text("Button State")
                        .font(.caption)
                        .foregroundColor(.secondary)

                    HStack {
                        Image(systemName: bleManager.buttonState == "PRESSED" ? "button.programmable" : "button.programmable.square")
                            .font(.system(size: 40))
                            .foregroundColor(bleManager.buttonState == "PRESSED" ? .orange : .gray)

                        Text(bleManager.buttonState)
                            .font(.system(size: 32, weight: .bold, design: .rounded))
                            .foregroundColor(bleManager.buttonState == "PRESSED" ? .orange : .primary)
                    }
                    .padding()
                    .frame(maxWidth: .infinity)
                    .background(
                        RoundedRectangle(cornerRadius: 12)
                            .fill(bleManager.buttonState == "PRESSED" ? Color.orange.opacity(0.1) : Color(.systemGray6))
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

                // Log
                VStack(alignment: .leading, spacing: 8) {
                    Text("Activity Log")
                        .font(.caption)
                        .foregroundColor(.secondary)

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

                Spacer()
            }
            .padding()
            .background(Color(.systemGroupedBackground))
            .navigationTitle("ESP32 Monitor")
        }
    }
}

#Preview {
    ContentView()
}
