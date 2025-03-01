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
                                onSendPacket(3, String(vent.id) + "." + position)
                            },
                            vent_ID: String(vent.id)
                        )
                    } else {
                        TemperatureControlView(
                            temperature: $tempAdjustment,
                            onTemperatureSet: { temp in
                                // Update target temperature in the vent
                                vent.targetTemp = String(format: "%.1f", temp)
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
                
                // Initialize manual position based on open/closed state
                manualPosition = vent.isOpen ? "100" : "0"
            }
        }
    }
}
