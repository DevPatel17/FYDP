import SwiftUI
import Network

struct ContentView: View {
    @State private var motorPosition: String = ""
    @State private var temperature: String = ""
    @State private var receivedPktType: String = ""
    @State private var receivedTemperature: String = ""

    var body: some View {
        VStack{
            Text("Easy Breezy Test Prototype").font(/*@START_MENU_TOKEN@*/.title/*@END_MENU_TOKEN@*/).monospaced()
            Text("Received Packet:")
                .font(.headline)
                .padding(.top)
            
            Text("Packet Type: \(receivedPktType)")
                .padding(.top)
            
            Text("Temperature: \(receivedTemperature)")
                .padding(.top)
            HStack {
                VStack{
                    TextField("Motor Position", text: $motorPosition)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                        .padding()
                    Button(action: {
                        if let motorPositionFloat = Float(motorPosition) {
                            sendPacket(pktType: 3, value: motorPositionFloat)
                        }
                    }) {
                        Text("Send Motor Position")
                            .padding()
                            .background(Color.blue)
                            .foregroundColor(.white)
                            .cornerRadius(10)
                    }
                }
                VStack{
                    TextField("Temperature", text: $temperature)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                        .padding()
                    Button(action: {
                        if let temperatureFloat = Float(temperature) {
                            sendPacket(pktType: 2, value: temperatureFloat)
                        }
                    }) {
                        Text("Send Temperature")
                            .padding()
                            .background(Color.blue)
                            .foregroundColor(.white)
                            .cornerRadius(10)
                    }
                }
            }
        }.padding().onAppear {
startListening()}
            
            
    }

    func sendPacket(pktType: Int, value: Float) {
        let connection = NWConnection(host: "192.168.0.19", port: 1234, using: .udp)
        connection.start(queue: .main)

        var pktTypeLE = UInt32(pktType).littleEndian
        var temperatureLE = value.bitPattern.littleEndian

        let pktTypeData = Data(bytes: &pktTypeLE, count: 4)
        let temperatureData = Data(bytes: &temperatureLE, count: 4)

        let packetData = pktTypeData + temperatureData
        // Improved Logging
        let packetDataHexString = packetData.map { String(format: "%02x", $0) }.joined()
        print("Packet Type (LE): \(pktTypeLE)")
        print("Temperature (bit pattern LE): \(temperatureLE)")
        print("Packet Data (Hex): \(packetDataHexString)")

        // Log byte-by-byte
        for (index, byte) in packetData.enumerated() {
            print(String(format: "Byte %d: %02x", index, byte))
        }

        connection.send(content: packetData, completion: .contentProcessed({ (error) in
            if let error = error {
                print("Error sending data: \(error)")
            } else {
                print("Data sent successfully")
            }
        }))
    }
    
    func startListening() {
        let listener = try! NWListener(using: .udp, on: 3001)
        
        listener.newConnectionHandler = { newConnection in
            newConnection.start(queue: .main)
            self.receive(on: newConnection)
        }
        
        listener.start(queue: .main)
    }
    
    func receive(on connection: NWConnection) {
        connection.receiveMessage { (data, context, isComplete, error) in
            if let data = data, !data.isEmpty {
                var pktType: UInt32 = 0
                var temperatureBitPattern: UInt32 = 0

                data.withUnsafeBytes { buffer in
                    let rawBytes = buffer.bindMemory(to: UInt8.self)
                    memcpy(&pktType, rawBytes.baseAddress!, MemoryLayout<UInt32>.size)
                    memcpy(&temperatureBitPattern, rawBytes.baseAddress! + MemoryLayout<UInt32>.size, MemoryLayout<UInt32>.size)
                }

                let pktTypeHost = UInt32(littleEndian: pktType)
                let temperature = Float(bitPattern: temperatureBitPattern)

                self.receivedPktType = String(pktTypeHost)
                self.receivedTemperature = String(temperature)

                print("Received Packet Type: \(pktTypeHost), Temperature: \(temperature)")
            }

            self.receive(on: connection)
        }
    }
}

    


struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}
