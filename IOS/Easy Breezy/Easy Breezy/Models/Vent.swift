//
//  Vent.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-20.
//

import SwiftUI

struct Vent: Identifiable, Codable {
    let id: Int
    let room: String
    var temperature: String
    var targetTemp: String
    var isOpen: Bool
    var isManualMode: Bool = false  // Default to temperature control mode
    var manualPosition: String = "0"  // Store the manual position value (0-100)
    
    // Color isn't stored in persistence since we now use gradients
    // This is kept for backward compatibility
    var color: Color {
        isOpen ? Color(hex: "86B5A5") : Color.gray.opacity(0.3)
    }
    
    // CodingKeys to specify which properties to encode/decode
    enum CodingKeys: String, CodingKey {
        case id, room, temperature, targetTemp, isOpen, isManualMode, manualPosition
    }
}
