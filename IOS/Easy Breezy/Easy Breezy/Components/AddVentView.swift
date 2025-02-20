//
//  AddVentView.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-20.
//

import SwiftUI


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
