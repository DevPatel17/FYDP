//
//  SettingsView.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-28.
//

import SwiftUI

struct SettingsView: View {
    @Environment(\.dismiss) var dismiss
    @ObservedObject var ventStore: VentStore
    @State private var showingResetConfirmation = false
    @State private var showingPlaceholderConfirmation = false
    
    var body: some View {
        NavigationView {
            List {
                Section(header: Text("App Settings")) {
                    // App version
                    HStack {
                        Text("Version")
                        Spacer()
                        Text("1.0.0")
                            .foregroundColor(.gray)
                    }
                    
                    // Add more settings options here as needed
                }
                
                Section(header: Text("Data Management")) {
                    Button(action: {
                        showingPlaceholderConfirmation = true
                    }) {

                            Text("Add Placeholder Vents")
                            .foregroundColor(.blue)
                    }
                    Button(action: {
                        showingResetConfirmation = true
                    }) {
                            Text("Clear All Vents")
                                .foregroundColor(.red)
                    }
                    
                }
                
                Section(header: Text("About")) {
                    // Information about the app
                    Text("Easy Breezy allows you to control your smart vents from your iOS device.")
                        .font(.caption)
                        .foregroundColor(.gray)
                }
            }
            .navigationTitle("Settings")
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Done") {
                        dismiss()
                    }
                }
            }
            .alert("Clear All Vents?", isPresented: $showingResetConfirmation) {
                Button("Cancel", role: .cancel) { }
                Button("Clear All", role: .destructive) {
                    ventStore.resetStore()
                }
            } message: {
                Text("This will remove all your vents. This action cannot be undone.")
            }.alert("Add Placeholders?", isPresented: $showingPlaceholderConfirmation) {
                Button("Cancel", role: .cancel) { }
                Button("Add Placeholder Vents") {
                    ventStore.addPlaceholderVents()
                }
            }
        }
    }
}
