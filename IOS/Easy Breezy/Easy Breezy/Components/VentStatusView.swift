//
//  VentStatusView.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-20.
//

import SwiftUI

struct VentStatusView: View {
    let temperature: String
    let targetTemp: String
    let isManualMode: Bool
    
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
                
                // Only show target temperature if NOT in manual mode
                if !isManualMode {
                    DataBadge(
                        icon: "target",
                        title: "Target",
                        value: targetTemp + "°C"
                    )
                }
            }
        }
        .padding()
        .background(Color.white.opacity(0.1))
        .cornerRadius(16)
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
    }
}
