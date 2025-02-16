        import SwiftUI
        import Network
        import CoreBluetooth

extension Notification.Name {
    static let ventSetupComplete = Notification.Name("ventSetupComplete")
}

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
                
                connection.send(content: packetData, completion: .contentProcessed { error in
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
                        
                        LazyVGrid(columns: [
                            GridItem(.flexible()),
                            GridItem(.flexible())
                        ], spacing: 16) {
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
                            var valueBitPattern: UInt32 = 0
                            
                            data.withUnsafeBytes { buffer in
                                let rawBytes = buffer.bindMemory(to: UInt8.self)
                                memcpy(&pktType, rawBytes.baseAddress!, MemoryLayout<UInt32>.size)
                                memcpy(&valueBitPattern, rawBytes.baseAddress! + MemoryLayout<UInt32>.size, MemoryLayout<UInt32>.size)
                            }
                            
                            let pktTypeHost = UInt32(littleEndian: pktType)
                            let value = Float(bitPattern: valueBitPattern)
                            
                            print("📦 Decoded packet - Type: \(pktTypeHost), Value: \(value)")
                            
                            // Update the UI
                            DispatchQueue.main.async {
                                self.receivedPktType = String(pktTypeHost)
                                self.receivedTemperature = String(format: "%.1f°", value)
                                
                                if pktTypeHost == 1 {
                                    // Store the value as ventID
                                    let ventID = Int(value)  // Convert float value to integer
                                    // Setup success received with ventID
                                    NotificationCenter.default.post(
                                        name: .ventSetupComplete,
                                        object: nil,
                                        userInfo: ["ventID": ventID]  // Pass the ventID in the notification
                                    )
                                } else if pktTypeHost <= 5 {
                                    // Update the corresponding vent's temperature
                                    if let ventIndex = self.vents.firstIndex(where: { $0.id == Int(pktTypeHost) }) {
                                        self.vents[ventIndex].temperature = String(format: "%.1f", value)
                                    }
                                }
                            }
                        }
                        self.receive(on: connection)
                    }
                }
        }

        // MARK: - Models
        struct Vent: Identifiable {
            let id: Int
            let room: String
            var temperature: String
            var targetTemp: String
            var isOpen: Bool
            
            var color: Color {
                isOpen ? Color(hex: "86B5A5") : Color.gray.opacity(0.3)
            }
        }

        // MARK: - Supporting Views
        struct VentCard: View {
            @Binding var vent: Vent
            let onTap: () -> Void
            let onToggle: () -> Void
            
            var body: some View {
                Button(action: onTap) {
                    VStack(alignment: .leading) {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(vent.room)
                                .font(.headline)
                                .foregroundColor(.white)
                            
                            Text(vent.isOpen ? "Open" : "Closed")
                                .font(.subheadline)
                                .foregroundColor(.gray)
                        }
                        
                        HStack {
                            VStack(alignment: .leading) {
                                Text(vent.temperature + "°C")
                                    .font(.system(.title3, design: .monospaced))
                                    .foregroundColor(.white)
                                
                                Text("Target: " + vent.targetTemp + "°C")
                                    .font(.caption)
                                    .foregroundColor(.gray)
                            }
                            
                            Spacer()
                            
                            Button(action: onToggle) {
                                Image(systemName: "power")
                                    .font(.system(size: 18))
                                    .foregroundColor(.white)
                                    .padding(8)
                                    .background(Color.white.opacity(0.2))
                                    .clipShape(RoundedRectangle(cornerRadius: 8))
                            }
                        }
                        .padding(.top, 8)
                    }
                    .padding()
                    .background(vent.color)
                    .cornerRadius(16)
                }
            }
        }

        // MARK: - Header Component
        struct VentHeaderView: View {
            let roomName: String
            let color: Color
            
            var body: some View {
                VStack(spacing: 12) {
                    Text(roomName + " Vent")
                        .font(.title3)
                        .fontWeight(.bold)
                        .foregroundColor(.white)
                    
                    Image(systemName: "fan.fill")
                        .font(.system(size: 60))
                        .foregroundColor(.white)
                        .padding(12)
                        .background(color.opacity(0.15))
                        .cornerRadius(16)
                }
                .padding()
                .background(
                    LinearGradient(
                        colors: [color, color.opacity(0.6)],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing
                    )
                )
                .cornerRadius(20)
            }
        }

        // MARK: - Status Component
        struct VentStatusView: View {
            let temperature: String
            let targetTemp: String
            
            var body: some View {
                VStack(spacing: 16) {
                    Text("Current Status")
                        .font(.headline)
                        .foregroundColor(.white)
                    
                    HStack(spacing: 24) {
                        DataBadge(
                            icon: "thermometer",
                            title: "Temperature",
                            value: temperature + "°C"
                        )
                        DataBadge(
                            icon: "target",
                            title: "Target",
                            value: targetTemp + "°C"
                        )
                    }
                }
                .padding()
                .background(Color.white.opacity(0.1))
                .cornerRadius(16)
            }
        }

        // MARK: - Temperature Control Component
        struct TemperatureControlView: View {
            @Binding var temperature: String
            let onTemperatureSet: (Float) -> Void
            
            private let minTemp: Double = 16
            private let maxTemp: Double = 30
            
            private var currentTemp: Double {
                guard let temp = Double(temperature) else {
                    return minTemp
                }
                return min(max(temp, minTemp), maxTemp)
            }
            
            var body: some View {
                VStack() {
                    ZStack {
                        
                        // Background Arc
                        Circle()
                            .trim(from: 0.5, to: 1.0)  // 180 degrees
                            .stroke(Color.white.opacity(0.1), lineWidth: 24)
                            .frame(width: 200, height: 200)
                        
                        // Temperature Arc
                        Circle()
                            .trim(from: 0.5, to: 0.5 + 0.5 * ((currentTemp - minTemp) / (maxTemp - minTemp)))
                            .stroke(
                                LinearGradient(
                                    colors: [Color(hex: "86B5A5"), Color(hex: "86B5A5").opacity(0.7)],
                                    startPoint: .leading,
                                    endPoint: .trailing
                                ),
                                style: StrokeStyle(lineWidth: 24, lineCap: .round)
                            )
                            .frame(width: 200, height: 200)
                        
                        // Temperature Display with +/- Buttons
                        VStack(spacing: 4) {
                            VStack(spacing: 4) {
                                Text("\(String(format: "%.1f", currentTemp))°")
                                    .font(.system(size: 42, weight: .medium, design: .rounded))
                                    .foregroundColor(.white)
                                
                                Text("TARGET")
                                    .font(.caption2)
                                    .foregroundColor(.gray)
                            }
                            HStack(spacing: 160) {
                                Button(action: {
                                    let newTemp = max(minTemp, currentTemp - 0.5)
                                    temperature = String(format: "%.1f", newTemp)
                                    onTemperatureSet(Float(newTemp))
                                }) {
                                    Image(systemName: "minus.circle.fill")
                                        .font(.system(size: 32))
                                        .foregroundColor(.white)
                                }
                            
                                
                                Button(action: {
                                    let newTemp = min(maxTemp, currentTemp + 0.5)
                                    temperature = String(format: "%.1f", newTemp)
                                    onTemperatureSet(Float(newTemp))
                                }) {
                                    Image(systemName: "plus.circle.fill")
                                        .font(.system(size: 32))
                                        .foregroundColor(.white)
                                }
                            }
                        }
                    }
                }
                .frame(maxWidth: .infinity)  // Make the container take full width
                .padding(.vertical, 50)
                .padding(.bottom, -20)
                .background(Color.white.opacity(0.05))
                .cornerRadius(14)
            }
        }
        // MARK: - Manual Control Component
        struct ManualControlView: View {
            @Binding var position: String
            let onPositionSet: (Float) -> Void
            @State private var sliderValue: Double = 0
            @State private var isDragging: Bool = false
            
            var body: some View {
                VStack(spacing: 20) {
                    Text("\(Int(sliderValue))%")
                        .font(.system(size: 42, weight: .medium, design: .rounded))
                        .foregroundColor(.white)
                    
                    // Custom Slider
                    GeometryReader { geometry in
                        ZStack(alignment: .leading) {
                            // Slider Background
                            RoundedRectangle(cornerRadius: 12)
                                .fill(Color.white.opacity(0.1))
                                .frame(height: 24)
                            
                            // Slider Fill
                            RoundedRectangle(cornerRadius: 12)
                                .fill(
                                    LinearGradient(
                                        colors: [Color(hex: "86B5A5"), Color(hex: "86B5A5").opacity(0.7)],
                                        startPoint: .leading,
                                        endPoint: .trailing
                                    )
                                )
                                .frame(width: max(24, geometry.size.width * sliderValue / 100), height: 24)
                            
                            // Slider Knob
                            Circle()
                                .fill(Color.white)
                                .frame(width: 32, height: 32)
                                .shadow(color: .black.opacity(0.15), radius: 5)
                                .offset(x: (geometry.size.width - 32) * sliderValue / 100)
                                .gesture(
                                    DragGesture()
                                        .onChanged { value in
                                            isDragging = true
                                            let newValue = min(max(0, value.location.x / geometry.size.width * 100), 100)
                                            sliderValue = newValue
                                            position = String(format: "%.0f", newValue)
                                        }
                                        .onEnded { _ in
                                            isDragging = false
                                            onPositionSet(Float(sliderValue))
                                        }
                                )
                        }
                    }
                    .frame(height: 32)
                    
                    // Labels
                    HStack {
                        Text("Closed")
                            .foregroundColor(.gray)
                        Spacer()
                        Text("Open")
                            .foregroundColor(.gray)
                    }
                    .font(.caption)
                }
                .padding()
                .background(Color.white.opacity(0.05))
                .cornerRadius(14)
                .onAppear {
                    sliderValue = Double(position) ?? 0
                }
            }
        }

        // MARK: - Main Detail View
        struct VentDetailView: View {
            @Binding var vent: Vent
            @Binding var receivedPktType: String
            @Binding var receivedTemperature: String
            let onSendPacket: (Int, Float) -> Void
            
            @Environment(\.dismiss) var dismiss
            @State private var tempAdjustment: String = ""
            @State private var manualPosition: String = ""
            @State private var isManualMode: Bool = false
            
            var body: some View {
                NavigationView {
                    ScrollView {
                        VStack(spacing: 24) {
                            VentHeaderView(roomName: vent.room, color: vent.color)
                            
                            VentStatusView(
                                temperature: vent.temperature,
                                targetTemp: vent.targetTemp
                            )
                            
                            Picker("Control Mode", selection: $isManualMode) {
                                Text("Temperature Mode").tag(false)
                                Text("Manual Mode").tag(true)
                            }
                            .pickerStyle(.segmented)
                            .padding(.horizontal)
                            
                            
                            if isManualMode {
                                ManualControlView(
                                    position: $manualPosition,
                                    onPositionSet: { position in
                                        onSendPacket(3, position)  // This will now use Bluetooth
                                    }
                                )
                            } else {
                                TemperatureControlView(
                                    temperature: $tempAdjustment,
                                    onTemperatureSet: { temp in
                                        vent.targetTemp = tempAdjustment
                                        onSendPacket(2, temp)  // This will now use Bluetooth
                                    }
                                )
                            }
                        }
                        .padding(.horizontal)
                    }
                    .background(Color(hex: "1C1C1E"))
                    .toolbar {
                        ToolbarItem(placement: .automatic) {
                            Button("Done") {
                                dismiss()
                            }
                        }
                    }
                    }
                }
            }
        struct DataBadge: View {
            let icon: String
            let title: String
            let value: String
            
            var body: some View {
                VStack(spacing: 6) {
                    Image(systemName: icon)
                        .font(.system(size: 18))
                        .foregroundColor(.white)
                    
                    Text(title)
                        .font(.caption2)
                        .foregroundColor(.gray)
                        .textCase(.uppercase)
                    
                    Text(value.isEmpty ? "---" : value)
                        .font(.system(.title3, design: .monospaced))
                        .foregroundColor(.white)
                }
                .padding(12)
                .frame(width: 120)
                .background(Color.white.opacity(0.05))
                .cornerRadius(12)
            }
        }

        // [Previous Color extension remains the same]
        extension Color {
            init(hex: String) {
                let hex = hex.trimmingCharacters(in: CharacterSet.alphanumerics.inverted)
                var int: UInt64 = 0
                Scanner(string: hex).scanHexInt64(&int)
                let a, r, g, b: UInt64
                switch hex.count {
                case 3: // RGB (12-bit)
                    (a, r, g, b) = (255, (int >> 8) * 17, (int >> 4 & 0xF) * 17, (int & 0xF) * 17)
                case 6: // RGB (24-bit)
                    (a, r, g, b) = (255, int >> 16, int >> 8 & 0xFF, int & 0xFF)
                case 8: // ARGB (32-bit)
                    (a, r, g, b) = (int >> 24, int >> 16 & 0xFF, int >> 8 & 0xFF, int & 0xFF)
                default:
                    (a, r, g, b) = (1, 1, 1, 0)
                }

                self.init(
                    .sRGB,
                    red: Double(r) / 255,
                    green: Double(g) / 255,
                    blue:  Double(b) / 255,
                    opacity: Double(a) / 255
                )
            }
        }
struct AddVentView: View {
    @Environment(\.dismiss) var dismiss
    @Binding var vents: [Vent]
    let onSendPacket: (Int, Float) -> Void
    
    @State private var ventName: String = ""
    @State private var setupStage: SetupStage = .initial
    @State private var setupError: String? = nil
    @State private var receivedVentID: Int?
    
    // Add observer for setup completion
    init(vents: Binding<[Vent]>, onSendPacket: @escaping (Int, Float) -> Void) {
        self._vents = vents
        self.onSendPacket = onSendPacket
        
        // Remove any existing observers to avoid duplicates
        NotificationCenter.default.removeObserver(self)
    }
    
    enum SetupStage {
        case initial
        case searching
        case naming
    }
    
    var body: some View {
        NavigationView {
            VStack(spacing: 20) {
                switch setupStage {
                case .initial:
                    VStack(spacing: 16) {
                        Image(systemName: "fan.fill")
                            .font(.system(size: 60))
                            .foregroundColor(.white)
                        
                        Text("Add New Vent")
                            .font(.title2)
                            .fontWeight(.bold)
                            .foregroundColor(.white)
                        
                        Text("Make sure your new vent is powered on and nearby")
                            .font(.subheadline)
                            .foregroundColor(.gray)
                            .multilineTextAlignment(.center)
                            .padding(.horizontal)
                        
                        Button(action: startSetup) {
                            Text("Start Setup")
                                .font(.headline)
                                .foregroundColor(.white)
                                .frame(maxWidth: .infinity)
                                .padding()
                                .background(Color(hex: "86B5A5"))
                                .cornerRadius(12)
                        }
                        .padding(.top)
                    }
                    
                case .searching:
                    VStack(spacing: 16) {
                        ProgressView()
                            .scaleEffect(1.5)
                            .padding()
                        
                        Text("Searching for new vent...")
                            .font(.headline)
                            .foregroundColor(.white)
                        
                        if let error = setupError {
                            Text(error)
                                .font(.subheadline)
                                .foregroundColor(.red)
                                .padding()
                        }
                    }
                    
                case .naming:
                    VStack(spacing: 16) {
                        Text("New Vent Found!")
                            .font(.headline)
                            .foregroundColor(.white)
                        
                        TextField("Enter vent name", text: $ventName)
                            .textFieldStyle(RoundedBorderTextFieldStyle())
                            .padding()
                        
                        Button(action: completeSetup) {
                            Text("Complete Setup")
                                .font(.headline)
                                .foregroundColor(.white)
                                .frame(maxWidth: .infinity)
                                .padding()
                                .background(Color(hex: "86B5A5"))
                                .cornerRadius(12)
                        }
                        .disabled(ventName.isEmpty)
                    }
                }
            }
            .padding()
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(Color(hex: "1C1C1E"))
            .navigationBarItems(trailing: Button("Cancel") {
                dismiss()
            })
            .onReceive(NotificationCenter.default.publisher(for: .ventSetupComplete)) { notification in
                print("Received setup complete notification")
                if let ventID = notification.userInfo?["ventID"] as? Int {
                    self.receivedVentID = ventID
                }
                setupStage = .naming
            }
        }
    }
    
    private func startSetup() {
        setupStage = .searching
        setupError = nil
        onSendPacket(1, 0.0)  // Changed from (7, 1.0) to (1, 0.0)
    }
    
    private func completeSetup() {
        let newVent = Vent(
            id: receivedVentID ?? vents.count,  // Use received ventID if available
            room: ventName,
            temperature: "20.0",
            targetTemp: "20.0",
            isOpen: false
        )
        vents.append(newVent)
        dismiss()
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
