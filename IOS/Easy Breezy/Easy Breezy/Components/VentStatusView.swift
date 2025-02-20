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
