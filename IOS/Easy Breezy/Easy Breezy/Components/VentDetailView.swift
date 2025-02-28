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
                                onSendPacket(3, String(position))  // This will now use Bluetooth
                            }
                        )
                    } else {
                        TemperatureControlView(
                            temperature: $tempAdjustment,
                            onTemperatureSet: { temp in
                                vent.targetTemp = tempAdjustment
                                onSendPacket(2, String(temp))  // This will now use Bluetooth
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
