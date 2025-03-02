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
                    HStack {
                        Text(vent.room)
                            .font(.headline)
                            .foregroundColor(.white)
                        
                        Spacer()
                        
                        // Mode indicator icon - only show if vent is open
                        if vent.isOpen {
                            Image(systemName: vent.isManualMode ? "slider.horizontal.3" : "thermometer")
                                .font(.system(size: 12))
                                .padding(6)
                                .foregroundColor(.white)
                                .background(
                                    vent.isManualMode ?
                                        Color.orange.opacity(0.7) :
                                        Color.blue.opacity(0.7)
                                )
                                .cornerRadius(6)
                        }
                    }
                    
                    Text(vent.isOpen ? "Open" : "Closed")
                        .font(.subheadline)
                        .foregroundColor(.white)
                }
                
                HStack {
                    VStack(alignment: .leading) {
                        Text(vent.temperature + "°C")
                            .font(.system(.title3, design: .monospaced))
                            .foregroundColor(.white)
                        
                        // Only show additional details if vent is open
                        if vent.isOpen {
                            if !vent.isManualMode {
                                Text("Target: " + vent.targetTemp + "°C")
                                    .font(.caption)
                                    .foregroundColor(.white)
                            } else {
                                Text("Position: " + vent.manualPosition + "%")
                                    .font(.caption)
                                    .foregroundColor(.white)
                            }
                        }
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
            .background(
                Group {
                    if vent.isOpen {
                        // Create a unique gradient based on vent ID
                        LinearGradient(
                            colors: [
                                Color(hex: "3498db").opacity(0.8 + Double(vent.id % 5) * 0.05),  // Blue with variation
                                Color(hex: "2ac9de").opacity(0.9 + Double(vent.id % 3) * 0.03)   // Cyan with variation
                            ],
                            startPoint: .topLeading,
                            endPoint: .bottomTrailing
                        )
                    } else {
                        Color.gray.opacity(0.3)
                    }
                }
            )
            .cornerRadius(16)
        }
    }
}
