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
    
    // Color is a computed property, not stored in persistence
    var color: Color {
        isOpen ? Color(hex: "86B5A5") : Color.gray.opacity(0.3)
    }
    
    // CodingKeys to specify which properties to encode/decode
    enum CodingKeys: String, CodingKey {
        case id, room, temperature, targetTemp, isOpen
    }
}
