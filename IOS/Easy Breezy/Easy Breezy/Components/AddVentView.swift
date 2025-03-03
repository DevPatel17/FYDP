//
//  AddVentView.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-20.
//

import SwiftUI


struct AddVentView: View {
    @Environment(\.dismiss) var dismiss
    // Use VentStore instead of Binding to [Vent]
    @ObservedObject var ventStore: VentStore
    var onSendPacket: (Int, String) -> Void
    
    @State private var ventName: String = ""
    @State private var setupStage: SetupStage = .initial
    @State private var setupError: String? = nil
    @State private var receivedVentID: Int?
    @State private var showingQRScanner = false
    @State private var scannedCode: String? = nil
    
    // Add observer for setup completion
    init(ventStore: VentStore, onSendPacket: @escaping (Int, String) -> Void) {
        self.ventStore = ventStore
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
                        
                        Button(action: { showingQRScanner = true }) {
                            HStack {
                                Image(systemName: "qrcode.viewfinder")
                                    .font(.system(size: 20))
                                Text("Scan QR Code")
                                    .font(.headline)
                            }
                            .foregroundColor(.white)
                            .frame(maxWidth: .infinity)
                            .padding()
                            .background(LinearGradient(
                                colors: [
                                    Color(hex: "3498db").opacity(0.8),  // Blue
                                    Color(hex: "2ac9de").opacity(0.9)   // Cyan
                                ],
                                startPoint: .leading,
                                endPoint: .trailing
                            ))
                            .cornerRadius(12)
                        }
                        .padding(.top)
                        
//                        Button(action: startSetupWithoutQR) {
//                            Text("Manual Setup")
//                                .font(.headline)
//                                .foregroundColor(.white.opacity(0.8))
//                                .frame(maxWidth: .infinity)
//                                .padding()
//                                .background(Color.white.opacity(0.1))
//                                .cornerRadius(12)
//                        }
//                        .padding(.top, 8)
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
                        
                        // Show the scanned code if available
                        if let code = scannedCode {
                            Text("Connecting to: \(code)")
                                .font(.subheadline)
                                .foregroundColor(.white.opacity(0.7))
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
            .background(
                LinearGradient(
                    gradient: Gradient(colors: [
                        Color(hex: "0F2942"),  // Deep blue that matches the accent
                        Color(hex: "071A2F")   // Very dark blue-green
                    ]),
                    startPoint: .topLeading,
                    endPoint: .bottomTrailing
                )
                .edgesIgnoringSafeArea(.all)
            )
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
            .sheet(isPresented: $showingQRScanner) {
                QRScannerView(onCodeScanned: { code in
                    self.scannedCode = code
                    showingQRScanner = false
                })
            }
            .onChange(of: showingQRScanner) { _, newValue in
                if newValue == false && scannedCode != nil {
                    // QR scanner was dismissed with a code
                    // Wait a small delay to ensure the view has fully transitioned
                    DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
                        startSetupWithQRCode(scannedCode!)
                    }
                }
            }
        }
    }
    
    private func startSetupWithQRCode(_ code: String) {
        print("QR Code scanned in AddVentView: \(code)")
        setupStage = .searching
        setupError = nil
        
        // Add additional verification
        print("About to send packet with type 1 and value: \(code)")
        
        // Send the scanned QR code data with packet type 1
        // Using a small delay to ensure view state has settled
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.2) {
            self.onSendPacket(1, code)
            print("Packet sending initiated")
        }
    }
    
    private func startSetupWithoutQR() {
        setupStage = .searching
        setupError = nil
        // Original behavior - send empty data
        onSendPacket(1, "0")
    }
    
    private func completeSetup() {
        // Use the next available vent ID if none was received
        let ventId = receivedVentID ?? ventStore.getNextVentId()
        
        let newVent = Vent(
            id: ventId,
            room: ventName,
            temperature: "20.0",
            targetTemp: "20.0",
            isOpen: false,
            isManualMode: false,  // Initialize to temperature control mode by default
            manualPosition: "0"   // Initialize manual position to 0 (closed)
        )
        
        // Add to the store (which handles persistence)
        ventStore.addVent(newVent)
        
        // Debug print to verify the vent was added
        print("Added new vent with ID: \(ventId), Name: \(ventName)")
        print("Total vents in store: \(ventStore.vents.count)")
        
        // Dismiss the sheet
        dismiss()
    }
}
