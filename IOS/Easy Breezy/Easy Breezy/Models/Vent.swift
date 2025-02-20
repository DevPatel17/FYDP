//
//  Vent.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-20.
//

import SwiftUI

struct Vent: Identifiable {
    let id: Int
    let room: String
    var temperature: String
    var targetTemp: String
    var isOpen: Bool
    
    var color: Color {
        isOpen ? Color(hex: "86B5A5") : Color.gray.opacity(0.3)
    }
}
