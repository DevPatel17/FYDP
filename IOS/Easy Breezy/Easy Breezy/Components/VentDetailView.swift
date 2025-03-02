//
//  VentDetailView.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-20.
//

import SwiftUI

struct VentDetailView: View {
    @Binding var vent: Vent
    @Binding var receivedPktType: String
    @Binding var receivedTemperature: String
    let onSendPacket: (Int, String) -> Void

    @Environment(\.dismiss) var dismiss
    @State private var tempAdjustment: String = ""
    @State private var localManualPosition: String = ""

    var body: some View {
        NavigationView {
            ScrollView {
                VStack(spacing: 24) {
                    VentHeaderView(roomName: vent.room, color: vent.color, ventId: vent.id)

                    VentStatusView(
                        temperature: vent.temperature,
                        targetTemp: vent.targetTemp,
                        isManualMode: vent.isManualMode
                    )

                    Picker("Control Mode", selection: $vent.isManualMode) {
                        Text("Temperature Mode").tag(false)
                        Text("Manual Mode").tag(true)
                    }
                    .pickerStyle(.segmented)
                    .padding(.horizontal)
                    .onChange(of: vent.isManualMode) { _, isManual in
                        if isManual {
                            // Switching to manual mode
                            localManualPosition = vent.manualPosition
                            
                            // Get position value
                            let positionValue = Int(vent.manualPosition) ?? 0
                            
                            // Ensure vent is open if position > 0
                            if positionValue > 0 {
                                vent.isOpen = true
                            }
                            
                            // Send a packet to update the system mode
                            onSendPacket(3, String(vent.id) + "." + vent.manualPosition)
                        } else {
                            // Switching to temperature mode
                            // Set vent to open state
                            vent.isOpen = true
                            
                            // Send target temperature to system
                            if let temp = Float(vent.targetTemp) {
                                onSendPacket(2, String(temp))
                            }
                        }
                    }

                    if vent.isManualMode {
                        ManualControlView(
                            position: $localManualPosition,
                            onPositionSet: { position in
                                // Update the model's manual position
                                vent.manualPosition = String(position.split(separator: ".").last ?? "0")
                                
                                // Get position value
                                let positionValue = Int(vent.manualPosition) ?? 0
                                
                                // Update isOpen based on position value
                                // Only set to closed if explicitly set to 0
                                vent.isOpen = positionValue > 0
                                
                                // Send a manual control packet (type 3)
                                onSendPacket(3, position)
                            },
                            vent_ID: String(vent.id)
                        )
                    } else {
                        TemperatureControlView(
                            temperature: $tempAdjustment,
                            onTemperatureSet: { temp in
                                // Update target temperature in the vent
                                vent.targetTemp = String(format: "%.1f", temp)
                                
                                // Set vent to open state when sending temperature
                                vent.isOpen = true
                                
                                // Send a temperature control packet (type 2)
                                onSendPacket(2, String(temp))
                            }
                        )
                    }
                }
                .padding(.horizontal)
            }
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
            .toolbar {
                ToolbarItem(placement: .automatic) {
                    Button("Done") {
                        dismiss()
                    }
                }
            }
            .onAppear {
                // Initialize temperature adjustment with current target
                tempAdjustment = vent.targetTemp
                
                // Initialize manual position from stored value
                localManualPosition = vent.manualPosition
            }
        }
    }
}
