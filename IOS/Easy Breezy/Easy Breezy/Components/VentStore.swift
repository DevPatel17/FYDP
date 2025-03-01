//  VentStore.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-28.
//

import SwiftUI

class VentStore: ObservableObject {
    @Published var vents: [Vent] = []
    private let saveKey = "SavedVents"
    
    init() {
        loadVents()
        
        // Only add placeholders if we couldn't load any vents from storage
        // This prevents overwriting actual data with placeholders if the array is empty for other reasons
        if vents.isEmpty && !UserDefaults.standard.bool(forKey: "VentsEverSaved") {
            // Add placeholder vents if none exist
            addPlaceholderVents()
        }
    }
    
    func loadVents() {
        if let data = UserDefaults.standard.data(forKey: saveKey) {
            if let decoded = try? JSONDecoder().decode([Vent].self, from: data) {
                // Ensure no duplicate IDs by using a dictionary
                var uniqueVentsDict: [Int: Vent] = [:]
                
                // Process each vent, keeping only the latest one with a given ID
                for vent in decoded {
                    uniqueVentsDict[vent.id] = vent
                }
                
                // Convert back to array
                vents = Array(uniqueVentsDict.values).sorted(by: { $0.id < $1.id })
                
                // Log the loaded vents
                print("Loaded \(vents.count) vents from storage")
                for vent in vents {
                    print("  - Vent ID: \(vent.id), Room: \(vent.room)")
                }
                
                return
            }
        }
        
        // If we reach here, no saved vents were found
        vents = []
    }
    
    func saveVents() {
        // Check for duplicate IDs before saving
        let idCounts = vents.reduce(into: [Int: Int]()) { counts, vent in
            counts[vent.id, default: 0] += 1
        }
        
        let duplicateIds = idCounts.filter { $0.value > 1 }.keys
        
        if !duplicateIds.isEmpty {
            print("WARNING: Found duplicate vent IDs: \(duplicateIds)")
            
            // Remove duplicates, keeping only the first occurrence of each ID
            var uniqueVentsDict: [Int: Vent] = [:]
            for vent in vents {
                if uniqueVentsDict[vent.id] == nil {
                    uniqueVentsDict[vent.id] = vent
                }
            }
            
            // Replace the array with deduplicated vents
            vents = Array(uniqueVentsDict.values).sorted(by: { $0.id < $1.id })
            print("Removed duplicates, now have \(vents.count) unique vents")
        }
        
        if let encoded = try? JSONEncoder().encode(vents) {
            UserDefaults.standard.set(encoded, forKey: saveKey)
            // Set a flag indicating that we have saved vents (to prevent placeholders)
            UserDefaults.standard.set(true, forKey: "VentsEverSaved")
            
            // Log the saved vents
            print("Saved \(vents.count) vents to storage")
        }
    }
    
    private func addPlaceholderVents() {
        // Clear any existing vents
        vents = []
        
        // Start with higher placeholder IDs to avoid conflicts with real vents
        vents = [
            Vent(id: 1000, room: "Living Room", temperature: "22.0", targetTemp: "21.5", isOpen: true, isManualMode: false),
            Vent(id: 1001, room: "Bedroom", temperature: "20.5", targetTemp: "20.0", isOpen: false, isManualMode: true),
            Vent(id: 1200, room: "Kitchen", temperature: "25.0", targetTemp: "21.5", isOpen: true, isManualMode: false),
            Vent(id: 12001, room: "Basement", temperature: "20.5", targetTemp: "20.0", isOpen: false, isManualMode: true),
            Vent(id: 10030, room: "Family Room", temperature: "22.0", targetTemp: "21.5", isOpen: true, isManualMode: false),
            Vent(id: 1401, room: "Garage", temperature: "10.5", targetTemp: "20.0", isOpen: false, isManualMode: true),
        ]
        
        print("Added placeholder vents with IDs 1000 and 1001")
        saveVents()
    }
    
    func addVent(_ vent: Vent) {
        // Ensure we're not adding a duplicate ID
        if vents.contains(where: { $0.id == vent.id }) {
            print("Warning: Attempted to add vent with duplicate ID: \(vent.id)")
            return
        }
        
        // Add the vent and save
        vents.append(vent)
        print("Added vent with ID: \(vent.id), Total vents: \(vents.count)")
        saveVents()
        
        // Explicitly trigger objectWillChange to ensure UI updates
        self.objectWillChange.send()
    }
    
    func updateVent(_ vent: Vent) {
        if let index = vents.firstIndex(where: { $0.id == vent.id }) {
            vents[index] = vent
            saveVents()
        }
    }
    
    func updateVentProperty(id: Int, updateBlock: (inout Vent) -> Void) {
        if let index = vents.firstIndex(where: { $0.id == id }) {
            var vent = vents[index]
            updateBlock(&vent)
            vents[index] = vent
            saveVents()
            
            // Explicitly trigger objectWillChange to ensure UI updates
            self.objectWillChange.send()
        } else {
            print("Warning: Tried to update property of non-existent vent with ID: \(id)")
        }
    }
    
    // Get the next available vent ID
    func getNextVentId() -> Int {
        // Find the highest existing ID and add 1
        let highestId = vents.map { $0.id }.max() ?? 0
        let nextId = max(100, highestId + 1)  // Start from 100 if empty, or increment from highest
        
        print("Generated next vent ID: \(nextId)")
        return nextId
    }
    
    // Helper method to get a binding for a specific vent
    func binding(for vent: Vent) -> Binding<Vent> {
        // Create a Binding that updates the vents array
        Binding<Vent>(
            get: {
                guard let index = self.vents.firstIndex(where: { $0.id == vent.id }) else {
                    print("Warning: Tried to get binding for non-existent vent with ID: \(vent.id)")
                    return vent // Return the original vent if we can't find it in the array
                }
                return self.vents[index]
            },
            set: { newValue in
                if let index = self.vents.firstIndex(where: { $0.id == vent.id }) {
                    self.vents[index] = newValue
                    print("Updated vent with ID: \(vent.id), new state: isOpen=\(newValue.isOpen)")
                    self.saveVents()
                } else {
                    print("Warning: Tried to update non-existent vent with ID: \(vent.id)")
                }
            }
        )
    }
    
    func resetStore() {
        print("Resetting vent store...")
        UserDefaults.standard.removeObject(forKey: saveKey)
        UserDefaults.standard.removeObject(forKey: "VentsEverSaved")
        vents = []
        addPlaceholderVents()
        objectWillChange.send()
    }
}
