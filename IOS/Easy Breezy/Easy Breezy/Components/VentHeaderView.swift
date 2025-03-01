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
    let ventId: Int
    
    init(roomName: String, color: Color, ventId: Int = 0) {
        self.roomName = roomName
        self.color = color
        self.ventId = ventId
    }
    
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
                colors: [
                    Color(hex: "3498db").opacity(0.8 + Double(ventId % 5) * 0.05),
                    Color(hex: "2ac9de").opacity(0.9 + Double(ventId % 3) * 0.03)
                ],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
        )
        .cornerRadius(20)
    }
}
