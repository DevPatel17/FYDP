//
//  AddVentView.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-20.
//  Updated on 2025-02-28.
//

import SwiftUI
import AVFoundation

struct AddVentView: View {
    @Environment(\.dismiss) var dismiss
    @Binding var vents: [Vent]
    var onSendPacket: (Int, String) -> Void
    
    @State private var ventName: String = ""
    @State private var setupStage: SetupStage = .initial
    @State private var setupError: String? = nil
    @State private var receivedVentID: Int?
    @State private var scannedQRCode: String? = nil
    
    // Add observer for setup completion
    init(vents: Binding<[Vent]>, onSendPacket: @escaping (Int, String) -> Void) {
        self._vents = vents
        self.onSendPacket = onSendPacket
        
        // Remove any existing observers to avoid duplicates
        NotificationCenter.default.removeObserver(self)
    }
    
    enum SetupStage {
        case initial
        case scanning    // New stage for QR scanning
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
                        
                        Text("Scan the QR code on your new vent to begin setup")
                            .font(.subheadline)
                            .foregroundColor(.gray)
                            .multilineTextAlignment(.center)
                            .padding(.horizontal)
                        
                        Button(action: { setupStage = .scanning }) {
                            Text("Scan QR Code")
                                .font(.headline)
                                .foregroundColor(.white)
                                .frame(maxWidth: .infinity)
                                .padding()
                                .background(Color(hex: "86B5A5"))
                                .cornerRadius(12)
                        }
                        .padding(.top)
                    }
                    
                case .scanning:
                    VStack {
                        Text("Scan QR Code")
                            .font(.headline)
                            .foregroundColor(.white)
                            .padding()
                        
                        ZStack {
                            QRScannerView(
                                onCodeScanned: { code in
                                    self.scannedQRCode = code
                                    self.startSetup()
                                },
                                errorMessage: $setupError
                            )
                            
                            // Scanner overlay
                            Rectangle()
                                .stroke(Color(hex: "86B5A5"), lineWidth: 3)
                                .frame(width: 200, height: 200)
                                .background(Color.clear)
                            
                            // Scanner corners
                            VStack {
                                HStack {
                                    Image(systemName: "l.square")
                                        .font(.system(size: 24))
                                        .foregroundColor(Color(hex: "86B5A5"))
                                    Spacer()
                                    Image(systemName: "l.square")
                                        .font(.system(size: 24))
                                        .rotationEffect(.degrees(90))
                                        .foregroundColor(Color(hex: "86B5A5"))
                                }
                                Spacer()
                                HStack {
                                    Image(systemName: "l.square")
                                        .font(.system(size: 24))
                                        .rotationEffect(.degrees(270))
                                        .foregroundColor(Color(hex: "86B5A5"))
                                    Spacer()
                                    Image(systemName: "l.square")
                                        .font(.system(size: 24))
                                        .rotationEffect(.degrees(180))
                                        .foregroundColor(Color(hex: "86B5A5"))
                                }
                            }
                            .frame(width: 250, height: 250)
                        }
                        .frame(height: 300)
                        .cornerRadius(12)
                        .padding()
                        
                        if let error = setupError {
                            Text(error)
                                .font(.subheadline)
                                .foregroundColor(.red)
                                .padding()
                        }
                        
                        Button(action: { setupStage = .initial }) {
                            Text("Cancel Scan")
                                .font(.headline)
                                .foregroundColor(.white)
                                .frame(maxWidth: .infinity)
                                .padding()
                                .background(Color.red.opacity(0.8))
                                .cornerRadius(12)
                        }
                        .padding(.horizontal)
                    }
                    
                case .searching:
                    VStack(spacing: 16) {
                        ProgressView()
                            .scaleEffect(1.5)
                            .padding()
                        
                        Text("Connecting to vent...")
                            .font(.headline)
                            .foregroundColor(.white)
                        
                        Text("QR Code: \(scannedQRCode ?? "Unknown")")
                            .font(.caption)
                            .foregroundColor(.gray)
                        
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
        
        // Use the scanned QR code as the payload for the type 1 packet
        if let qrCode = scannedQRCode {
            onSendPacket(1, qrCode)  // Send scanned QR code as data with packet type 1
            print("Sent setup packet with QR code: \(qrCode)")
        } else {
            // Fallback to a default value if no QR code was scanned
            setupError = "No QR code was scanned. Please try again."
            setupStage = .initial
        }
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
