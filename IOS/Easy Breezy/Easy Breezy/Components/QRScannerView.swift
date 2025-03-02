//
//  QRScannerView.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-28.
//

import SwiftUI
import AVFoundation
import AudioToolbox

struct QRScannerView: View {
    @Environment(\.dismiss) var dismiss
    @State private var isShowingCamera = false
    @State private var scannedCode: String?
    @State private var showAlert = false
    
    // Simple callback when code is scanned
    var onCodeScanned: (String) -> Void
    
    var body: some View {
        ZStack {
            Color(hex: "1C1C1E").edgesIgnoringSafeArea(.all)
            
            VStack(spacing: 16) {
                Text("Scan Vent QR Code")
                    .font(.title2)
                    .fontWeight(.bold)
                    .foregroundColor(.white)
                
                Text("Position the QR code within the frame to set up your vent")
                    .font(.subheadline)
                    .foregroundColor(.gray)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal)
                
                if isShowingCamera {
                    QRCodeCameraView(scannedCode: $scannedCode)
                        .frame(height: 600)
                        .clipShape(RoundedRectangle(cornerRadius: 16))
                        .padding()
                        .onChange(of: scannedCode) { _, newValue in
                            if let code = newValue {
                                // First process the scanned code
                                print("QR code scanned in view: \(code)")
                                
                                // Provide feedback
                                AudioServicesPlaySystemSound(SystemSoundID(kSystemSoundID_Vibrate))
                                
                                // Pass the code back to the parent view
                                onCodeScanned(code)
                            }
                        }
                } else {
                    Button(action: {
                        // Check for camera permission first
                        checkCameraPermission()
                    }) {
                        VStack {
                            Image(systemName: "qrcode.viewfinder")
                                .font(.system(size: 60))
                                .foregroundColor(.white)
                            
                            Text("Start Scanning")
                                .font(.headline)
                                .foregroundColor(.white)
                                .padding(.top, 8)
                        }
                        .frame(maxWidth: .infinity)
                        .padding()
                        .background(Color(hex: "86B5A5"))
                        .cornerRadius(12)
                        .padding(.horizontal)
                    }
                }
                
                Button("Cancel") {
                    dismiss()
                }
                .foregroundColor(.white)
                .padding(.top)
            }
            .alert(isPresented: $showAlert) {
                Alert(
                    title: Text("Camera Access Required"),
                    message: Text("Please allow camera access in Settings to scan QR codes."),
                    primaryButton: .default(Text("Settings"), action: {
                        // Open app settings
                        if let settingsURL = URL(string: UIApplication.openSettingsURLString) {
                            UIApplication.shared.open(settingsURL)
                        }
                    }),
                    secondaryButton: .cancel()
                )
            }
        }
    }
    
    private func checkCameraPermission() {
        switch AVCaptureDevice.authorizationStatus(for: .video) {
        case .authorized:
            // Already authorized
            isShowingCamera = true
        case .notDetermined:
            // Request permission
            AVCaptureDevice.requestAccess(for: .video) { granted in
                DispatchQueue.main.async {
                    if granted {
                        isShowingCamera = true
                    } else {
                        showAlert = true
                    }
                }
            }
        case .denied, .restricted:
            // Show alert to go to settings
            showAlert = true
        @unknown default:
            // Handle any future cases
            showAlert = true
        }
    }
}

// Camera view for QR code scanning
struct QRCodeCameraView: UIViewRepresentable {
    @Binding var scannedCode: String?
    
    let captureSession = AVCaptureSession()
    
    func makeUIView(context: Context) -> UIView {
        let view = UIView(frame: UIScreen.main.bounds)
        view.backgroundColor = UIColor.black
        
        guard let videoCaptureDevice = AVCaptureDevice.default(for: .video) else { return view }
        
        // Configure capture session
        do {
            let videoInput = try AVCaptureDeviceInput(device: videoCaptureDevice)
            if captureSession.canAddInput(videoInput) {
                captureSession.addInput(videoInput)
            } else {
                return view
            }
            
            let metadataOutput = AVCaptureMetadataOutput()
            if captureSession.canAddOutput(metadataOutput) {
                captureSession.addOutput(metadataOutput)
                
                metadataOutput.setMetadataObjectsDelegate(context.coordinator, queue: DispatchQueue.main)
                metadataOutput.metadataObjectTypes = [.qr]
            } else {
                return view
            }
            
            // Add preview layer
            let previewLayer = AVCaptureVideoPreviewLayer(session: captureSession)
            previewLayer.frame = view.layer.bounds
            previewLayer.videoGravity = .resizeAspectFill
            view.layer.addSublayer(previewLayer)
            
            // Start capture session on background thread
            DispatchQueue.global(qos: .userInitiated).async {
                self.captureSession.startRunning()
            }
        } catch {
            print("Error setting up camera: \(error)")
        }
        
        return view
    }
    
    func updateUIView(_ uiView: UIView, context: Context) {
        // No updates needed
    }
    
    func makeCoordinator() -> Coordinator {
        Coordinator(self)
    }
    
    class Coordinator: NSObject, AVCaptureMetadataOutputObjectsDelegate {
        var parent: QRCodeCameraView
        var codeProcessed = false
        
        init(_ parent: QRCodeCameraView) {
            self.parent = parent
        }
        
        func metadataOutput(_ output: AVCaptureMetadataOutput, didOutput metadataObjects: [AVMetadataObject], from connection: AVCaptureConnection) {
            guard !codeProcessed,
                  let metadataObject = metadataObjects.first,
                  let readableObject = metadataObject as? AVMetadataMachineReadableCodeObject,
                  let stringValue = readableObject.stringValue else { return }
            
            // Prevent multiple processing of the same code
            codeProcessed = true
            
            print("QR code detected: \(stringValue)")
            
            // Update the binding on the main thread
            DispatchQueue.main.async {
                self.parent.scannedCode = stringValue
                
                // Stop the capture session after getting a valid code
                DispatchQueue.global(qos: .userInitiated).async {
                    self.parent.captureSession.stopRunning()
                }
            }
        }
    }
}
