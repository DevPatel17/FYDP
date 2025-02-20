//
//  TemperatureControlView.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-20.
//

import SwiftUI

struct TemperatureControlView: View {
    @Binding var temperature: String
    let onTemperatureSet: (Float) -> Void
    
    private let minTemp: Double = 16
    private let maxTemp: Double = 30
    
    private var currentTemp: Double {
        guard let temp = Double(temperature) else {
            return minTemp
        }
        return min(max(temp, minTemp), maxTemp)
    }
    
    var body: some View {
        VStack() {
            ZStack {
                
                // Background Arc
                Circle()
                    .trim(from: 0.5, to: 1.0)  // 180 degrees
                    .stroke(Color.white.opacity(0.1), lineWidth: 24)
                    .frame(width: 200, height: 200)
                
                // Temperature Arc
                Circle()
                    .trim(from: 0.5, to: 0.5 + 0.5 * ((currentTemp - minTemp) / (maxTemp - minTemp)))
                    .stroke(
                        LinearGradient(
                            colors: [Color(hex: "86B5A5"), Color(hex: "86B5A5").opacity(0.7)],
                            startPoint: .leading,
                            endPoint: .trailing
                        ),
                        style: StrokeStyle(lineWidth: 24, lineCap: .round)
                    )
                    .frame(width: 200, height: 200)
                
                // Temperature Display with +/- Buttons
                VStack(spacing: 4) {
                    VStack(spacing: 4) {
                        Text("\(String(format: "%.1f", currentTemp))°")
                            .font(.system(size: 42, weight: .medium, design: .rounded))
                            .foregroundColor(.white)
                        
                        Text("TARGET")
                            .font(.caption2)
                            .foregroundColor(.gray)
                    }
                    HStack(spacing: 160) {
                        Button(action: {
                            let newTemp = max(minTemp, currentTemp - 0.5)
                            temperature = String(format: "%.1f", newTemp)
                            onTemperatureSet(Float(newTemp))
                        }) {
                            Image(systemName: "minus.circle.fill")
                                .font(.system(size: 32))
                                .foregroundColor(.white)
                        }
                    
                        
                        Button(action: {
                            let newTemp = min(maxTemp, currentTemp + 0.5)
                            temperature = String(format: "%.1f", newTemp)
                            onTemperatureSet(Float(newTemp))
                        }) {
                            Image(systemName: "plus.circle.fill")
                                .font(.system(size: 32))
                                .foregroundColor(.white)
                        }
                    }
                }
            }
        }
        .frame(maxWidth: .infinity)  // Make the container take full width
        .padding(.vertical, 50)
        .padding(.bottom, -20)
        .background(Color.white.opacity(0.05))
        .cornerRadius(14)
    }
}
