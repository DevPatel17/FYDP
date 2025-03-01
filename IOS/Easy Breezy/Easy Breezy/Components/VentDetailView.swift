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

                    if vent.isManualMode {
                        ManualControlView(
                            position: $localManualPosition,
                            onPositionSet: { position in
                                // Update the model's manual position
                                vent.manualPosition = String(position.split(separator: ".").last ?? "0")
                                
                                // Update isOpen based on position value
                                let positionValue = Int(vent.manualPosition) ?? 0
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
                                // Send a temperature control packet (type 2)
                                onSendPacket(2, String(temp))
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
            .onAppear {
                // Initialize temperature adjustment with current target
                tempAdjustment = vent.targetTemp
                
                // Initialize manual position from stored value
                localManualPosition = vent.manualPosition
            }
            // When manual mode changes, update the local position
            .onChange(of: vent.isManualMode) { _, isManual in
                if isManual {
                    localManualPosition = vent.manualPosition
                }
            }
        }
    }
}
