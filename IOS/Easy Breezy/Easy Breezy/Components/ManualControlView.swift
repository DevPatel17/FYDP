//
//  ManualControlView.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-20.
//

import SwiftUI

struct ManualControlView: View {
    @Binding var position: String
    let onPositionSet: (Float) -> Void
    @State private var sliderValue: Double = 0
    @State private var isDragging: Bool = false
    
    var body: some View {
        VStack(spacing: 20) {
            Text("\(Int(sliderValue))%")
                .font(.system(size: 42, weight: .medium, design: .rounded))
                .foregroundColor(.white)
            
            // Custom Slider
            GeometryReader { geometry in
                ZStack(alignment: .leading) {
                    // Slider Background
                    RoundedRectangle(cornerRadius: 12)
                        .fill(Color.white.opacity(0.1))
                        .frame(height: 24)
                    
                    // Slider Fill
                    RoundedRectangle(cornerRadius: 12)
                        .fill(
                            LinearGradient(
                                colors: [Color("86B5A5"), Color("86B5A5").opacity(0.7)],
                                startPoint: .leading,
                                endPoint: .trailing
                            )
                        )
                        .frame(width: max(24, geometry.size.width * sliderValue / 100), height: 24)
                    
                    // Slider Knob
                    Circle()
                        .fill(Color.white)
                        .frame(width: 32, height: 32)
                        .shadow(color: .black.opacity(0.15), radius: 5)
                        .offset(x: (geometry.size.width - 32) * sliderValue / 100)
                        .gesture(
                            DragGesture()
                                .onChanged { value in
                                    isDragging = true
                                    let newValue = min(max(0, value.location.x / geometry.size.width * 100), 100)
                                    sliderValue = newValue
                                    position = String(format: "%.0f", newValue)
                                }
                                .onEnded { _ in
                                    isDragging = false
                                    onPositionSet(Float(sliderValue))
                                }
                        )
                }
            }
            .frame(height: 32)
            
            // Labels
            HStack {
                Text("Closed")
                    .foregroundColor(.gray)
                Spacer()
                Text("Open")
                    .foregroundColor(.gray)
            }
            .font(.caption)
        }
        .padding()
        .background(Color.white.opacity(0.05))
        .cornerRadius(14)
        .onAppear {
            sliderValue = Double(position) ?? 0
        }
    }
}
