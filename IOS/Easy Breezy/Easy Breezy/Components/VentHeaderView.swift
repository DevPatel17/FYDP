//
//  VentHeaderView.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-20.
//

import SwiftUI

struct VentHeaderView: View {
    let roomName: String
    let color: Color
    
    var body: some View {
        VStack(spacing: 12) {
            Text(roomName + " Vent")
                .font(.title3)
                .fontWeight(.bold)
                .foregroundColor(.white)
            
            Image(systemName: "fan.fill")
                .font(.system(size: 60))
                .foregroundColor(.white)
                .padding(12)
                .background(color.opacity(0.15))
                .cornerRadius(16)
        }
        .padding()
        .background(
            LinearGradient(
                colors: [color, color.opacity(0.6)],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
        )
        .cornerRadius(20)
    }
}
