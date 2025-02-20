import CoreBluetooth
import Network
import SwiftUI

struct ContentView: View {
    @State private var vents: [Vent] = []  // Start with empty array
    @State private var selectedVent: Vent? = nil
    @State private var receivedPktType: String = ""
    @State private var receivedTemperature: String = ""
    @State private var showingSettings = false
    @State private var showingAddVent = false

    func sendPacket(pktType: Int, value: Float) {
        let connection = NWConnection(
            host: "192.168.4.195",
            port: 5000,
            using: .udp
        )

        connection.start(queue: .main)

        var pktTypeLE = UInt32(pktType).littleEndian
        var valueLE = value.bitPattern.littleEndian

        let pktTypeData = Data(bytes: &pktTypeLE, count: 4)
        let valueData = Data(bytes: &valueLE, count: 4)
        let packetData = pktTypeData + valueData

        connection.send(
            content: packetData,
            completion: .contentProcessed { error in
                if let error = error {
                    print("❌ Send error: \(error.localizedDescription)")
                } else {
                    print("✅ Packet sent - Type: \(pktType), Value: \(value)")
                }
                connection.cancel()
            })
    }

    var body: some View {
        NavigationView {
            mainView
                .sheet(item: $selectedVent) { vent in
                    VentDetailView(
                        vent: binding(for: vent),
                        receivedPktType: $receivedPktType,
                        receivedTemperature: $receivedTemperature,
                        onSendPacket: sendPacket
                    )
                }
        }
        .onAppear(perform: startListening)
    }

    private var mainView: some View {
        ScrollView {
            VStack(spacing: 20) {
                headerView
                buildingStatusView

                LazyVGrid(
                    columns: [
                        GridItem(.flexible()),
                        GridItem(.flexible()),
                    ], spacing: 16
                ) {
                    ForEach($vents) { $vent in
                        VentCard(
                            vent: $vent,
                            onTap: { selectedVent = vent },
                            onToggle: {
                                vent.isOpen.toggle()
                                sendPacket(
                                    pktType: Int(UInt32(vent.id)),
                                    value: vent.isOpen ? 1.0 : 0.0
                                )
                            }
                        )
                    }

                    // Add Vent Button
                    Button(action: { showingAddVent = true }) {
                        VStack {
                            Image(systemName: "plus.circle.fill")
                                .font(.system(size: 40))
                            Text("Add Vent")
                                .font(.headline)
                        }
                        .foregroundColor(.white)
                        .frame(maxWidth: .infinity)
                        .frame(height: 160)
                        .background(Color.white.opacity(0.1))
                        .cornerRadius(16)
                    }
                }
            }
            .padding()
        }
        .background(Color(hex: "1C1C1E"))
        .sheet(isPresented: $showingAddVent) {
            AddVentView(vents: $vents, onSendPacket: sendPacket)
        }
    }

    private var headerView: some View {
        HStack {
            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text("Easy Breezy")
                        .font(.title2)
                        .fontWeight(.bold)
                        .foregroundColor(.white)
                }

                Text("14 Sage Street")
                    .font(.subheadline)
                    .foregroundColor(.gray)
            }

            Spacer()

            HStack(spacing: 4) {
                Text(averageTemperature)
                    .font(.title3)
                    .foregroundColor(.white)

                Image(systemName: "thermometer")
                    .foregroundColor(.white)
            }
        }
    }

    private var buildingStatusView: some View {
        HStack {
            VStack(alignment: .leading, spacing: 4) {
                Text("All Vents")
                    .font(.title3)
                    .fontWeight(.bold)
                    .foregroundColor(.white)

                Text("\(vents.count) Devices")
                    .font(.subheadline)
                    .foregroundColor(.gray)
            }

            Spacer()

            Button(action: { showingSettings.toggle() }) {
                Image(systemName: "gearshape.fill")
                    .font(.title3)
                    .foregroundColor(.gray)
            }
        }
    }

    private var averageTemperature: String {
        let sum = vents.compactMap { Float($0.temperature) }.reduce(0, +)
        let avg = sum / Float(vents.count)
        return String(format: "%.1f°C", avg)
    }

    // Helper function to get binding for specific vent
    private func binding(for vent: Vent) -> Binding<Vent> {
        guard let index = vents.firstIndex(where: { $0.id == vent.id }) else {
            fatalError("Vent not found")
        }
        return $vents[index]
    }

    // MARK: - Network Functions
    func startListening() {
        let listener = try! NWListener(using: .udp, on: 3001)
        listener.newConnectionHandler = { newConnection in
            newConnection.start(queue: .main)
            self.receive(on: newConnection)
        }
        listener.start(queue: .main)
    }

    func receive(on connection: NWConnection) {
        print("Starting receive function...")
        connection.receiveMessage { (data, context, isComplete, error) in
            if let error = error {
                print("❌ Receive error: \(error.localizedDescription)")
                return
            }

            print("Message received - isComplete: \(isComplete)")
            print("Data received: \(data?.count ?? 0) bytes")
            if let data = data, !data.isEmpty {
                var pktType: UInt32 = 0
                let stringData: Data

                // Get the packet type
                data.withUnsafeBytes { buffer in
                    let rawBytes = buffer.bindMemory(to: UInt8.self)
                    memcpy(
                        &pktType, rawBytes.baseAddress!,
                        MemoryLayout<UInt32>.size)
                }

                // Extract the string data after the packet type
                let startIndex = MemoryLayout<UInt32>.size
                stringData = data.subdata(in: startIndex..<data.count)

                let pktTypeHost = UInt32(littleEndian: pktType)

                if let valueStr = String(data: stringData, encoding: .utf8) {
                    print(
                        "📦 Decoded packet - Type: \(pktTypeHost), Value: \(valueStr)"
                    )

                    // Update the UI
                    DispatchQueue.main.async {
                        self.receivedPktType = String(pktTypeHost)

                        if pktTypeHost == 2 {
                            // Parse string in format "ventID.temperature"
                            let components = valueStr.split(separator: ".")
                            print("Split components: \(components)")  // Add debug print

                            // For "1.25.0", components will be ["1", "25", "0"]
                            if components.count >= 3,
                                let ventId = Int(components[0]),
                                let temperature = Float(
                                    components[1] + "." + components[2])
                            {
                                print(
                                    "Parsed ventId: \(ventId), temperature: \(temperature)"
                                )  // Add debug print

                                // Find and update the vent with matching ID
                                if let ventIndex = self.vents.firstIndex(
                                    where: { $0.id == ventId })
                                {
                                    self.vents[ventIndex].temperature = String(
                                        format: "%.1f", temperature)
                                    print(
                                        "Updated vent \(ventId) temperature to \(temperature)"
                                    )  // Add debug print
                                } else {
                                    print(
                                        "Could not find vent with ID \(ventId)")  // Add debug print
                                }
                            } else {
                                print(
                                    "Failed to parse components: \(components)")  // Add debug print
                            }
                        } else if pktTypeHost == 1 {
                            // Setup success received with VentID in string format
                            if let ventId = Int(valueStr) {  // Convert string to integer
                                NotificationCenter.default.post(
                                    name: .ventSetupComplete,
                                    object: nil,
                                    userInfo: ["ventID": ventId]
                                )
                            }
                        }
                    }
                }
            }
            self.receive(on: connection)
        }
    }
}



struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
            .previewDevice("iPhone 15 Pro")
            .previewDisplayName("iPhone 15 Pro")
            .environment(\.colorScheme, .dark)
    }
}
