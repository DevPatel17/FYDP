//
//  DataBadge.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-20.
//

import SwiftUI

struct DataBadge: View {
    let icon: String
    let title: String
    let value: String
    
    var body: some View {
        VStack(spacing: 6) {
            Image(systemName: icon)
                .font(.system(size: 18))
                .foregroundColor(.white)
            
            Text(title)
                .font(.caption2)
                .foregroundColor(.gray)
                .textCase(.uppercase)
            
            Text(value.isEmpty ? "---" : value)
                .font(.system(.title3, design: .monospaced))
                .foregroundColor(.white)
        }
        .padding(12)
        .frame(width: 120)
        .background(Color.white.opacity(0.05))
        .cornerRadius(12)
    }
}
