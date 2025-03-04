//
//  TemperatureControlView.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-20.
//

import SwiftUI
import UIKit

struct TemperatureControlView: View {
    @Binding var temperature: String
    let onTemperatureSet: (Float) -> Void
    
    @State private var isDragging: Bool = false
    @State private var tempDuringDrag: String = ""
    @State private var sliderValue: Double = 0.5
    
    private let minTemp: Double = 16
    private let maxTemp: Double = 30
    private let feedbackGenerator = UIImpactFeedbackGenerator(style: .light)
    @State private var lastFeedbackValue: Double = 0
    
    private var currentTemp: Double {
        guard let temp = Double(temperature) else {
            return minTemp
        }
        return min(max(temp, minTemp), maxTemp)
    }
    
    var body: some View {
        VStack(spacing: 20) {
            // Temperature display
            Text("\(isDragging ? tempDuringDrag : temperature)°")
                .font(.system(size: 48, weight: .medium, design: .rounded))
                .foregroundColor(.white)
            
            Text("TARGET TEMPERATURE")
                .font(.caption)
                .foregroundColor(.gray)
                .padding(.bottom, 10)
            
            // Custom slider
            GeometryReader { geometry in
                ZStack(alignment: .leading) {
                    // Track background
                    RoundedRectangle(cornerRadius: 10)
                        .fill(Color.white.opacity(0.1))
                        .frame(height: 20)
                    
                    // Temperature gradient
                    LinearGradient(
                        colors: [
                            Color(hex: "3498db").opacity(0.8), // Blue
                            Color(hex: "2ac9de").opacity(0.9)  // Cyan
                        ],
                        startPoint: .leading,
                        endPoint: .trailing
                    )
                    .clipShape(RoundedRectangle(cornerRadius: 10))
                    .frame(width: max(0, geometry.size.width * sliderValue), height: 20)
                    
                    // Knob
                    Circle()
                        .fill(Color.white)
                        .frame(width: 34, height: 34)
                        .shadow(color: .black.opacity(0.2), radius: 2)
                        .offset(x: max(0, min(geometry.size.width - 34, geometry.size.width * sliderValue - 17)))
                        .gesture(
                            DragGesture()
                                .onChanged { value in
                                    isDragging = true
                                    
                                    // Calculate position percentage (0 to 1)
                                    let width = geometry.size.width - 34
                                    let dragLocation = value.location.x - 17
                                    
                                    // Clamp between 0 and 1
                                    sliderValue = max(0, min(1, dragLocation / width))
                                    
                                    // Map to temperature range
                                    let tempRange = maxTemp - minTemp
                                    let rawTemp = minTemp + tempRange * sliderValue
                                    
                                    // Round to nearest 0.5°C
                                    let roundedTemp = round(rawTemp * 2) / 2
                                    
                                    // Clamp to valid range and update
                                    let clampedTemp = min(max(roundedTemp, minTemp), maxTemp)
                                    tempDuringDrag = String(format: "%.1f", clampedTemp)
                                    
                                    // Provide haptic feedback on 0.5 degree changes
                                    if abs(clampedTemp - lastFeedbackValue) >= 0.5 {
                                        feedbackGenerator.prepare()
                                        feedbackGenerator.impactOccurred()
                                        lastFeedbackValue = clampedTemp
                                    }
                                }
                                .onEnded { _ in
                                    // Parse the display value back to double
                                    if let finalTemp = Double(tempDuringDrag) {
                                        // Update binding and send temperature
                                        temperature = tempDuringDrag
                                        feedbackGenerator.impactOccurred(intensity: 1.0)
                                        onTemperatureSet(Float(finalTemp))
                                    }
                                    
                                    isDragging = false
                                }
                        )
                }
            }
            .frame(height: 40)
            .padding(.horizontal)
            
            // Temperature labels
            HStack {
                Text("\(Int(minTemp))°")
                    .font(.caption)
                    .foregroundColor(.gray)
                
                Spacer()
                
                Text("\(Int((minTemp + maxTemp) / 2))°")
                    .font(.caption)
                    .foregroundColor(.gray)
                
                Spacer()
                
                Text("\(Int(maxTemp))°")
                    .font(.caption)
                    .foregroundColor(.gray)
            }
            .padding(.horizontal, 10)
        }
        .padding()
        .background(Color.white.opacity(0.05))
        .cornerRadius(14)
        .onAppear {
            // Initialize tempDuringDrag with current temperature
            tempDuringDrag = temperature
            
            // Calculate initial slider position
            if let temp = Double(temperature) {
                let normalizedTemp = (temp - minTemp) / (maxTemp - minTemp)
                sliderValue = min(max(normalizedTemp, 0), 1)
            }
        }
        .onChange(of: temperature) { _, newValue in
            // Keep internal values in sync with external temperature changes
            if !isDragging {
                tempDuringDrag = newValue
                
                if let temp = Double(newValue) {
                    let normalizedTemp = (temp - minTemp) / (maxTemp - minTemp)
                    sliderValue = min(max(normalizedTemp, 0), 1)
                }
            }
        }
    }
}
