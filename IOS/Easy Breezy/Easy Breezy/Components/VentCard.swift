//
//  VentCard.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-20.
//

import SwiftUI

struct VentCard: View {
    @Binding var vent: Vent
    let onTap: () -> Void
    let onToggle: () -> Void
    
    var body: some View {
        Button(action: onTap) {
            VStack(alignment: .leading) {
                VStack(alignment: .leading, spacing: 4) {
                    Text(vent.room)
                        .font(.headline)
                        .foregroundColor(.white)
                    
                    Text(vent.isOpen ? "Open" : "Closed")
                        .font(.subheadline)
                        .foregroundColor(.gray)
                }
                
                HStack {
                    VStack(alignment: .leading) {
                        Text(vent.temperature + "°C")
                            .font(.system(.title3, design: .monospaced))
                            .foregroundColor(.white)
                        
                        Text("Target: " + vent.targetTemp + "°C")
                            .font(.caption)
                            .foregroundColor(.gray)
                    }
                    
                    Spacer()
                    
                    Button(action: onToggle) {
                        Image(systemName: "power")
                            .font(.system(size: 18))
                            .foregroundColor(.white)
                            .padding(8)
                            .background(Color.white.opacity(0.2))
                            .clipShape(RoundedRectangle(cornerRadius: 8))
                    }
                }
                .padding(.top, 8)
            }
            .padding()
            .background(vent.color)
            .cornerRadius(16)
        }
    }
}
