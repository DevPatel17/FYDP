//
//  QRScannerView.swift
//  Easy Breezy
//
//  Created by Ohm Srivastava on 2025-02-28.
//

import SwiftUI
import AVFoundation

struct QRScannerView: UIViewRepresentable {
    var onCodeScanned: (String) -> Void
    @Binding var errorMessage: String?
    
    class Coordinator: NSObject, AVCaptureMetadataOutputObjectsDelegate {
        var parent: QRScannerView
        
        init(parent: QRScannerView) {
            self.parent = parent
        }
        
        func metadataOutput(_ output: AVCaptureMetadataOutput, didOutput metadataObjects: [AVMetadataObject], from connection: AVCaptureConnection) {
            if let metadataObject = metadataObjects.first as? AVMetadataMachineReadableCodeObject {
                guard let stringValue = metadataObject.stringValue else { return }
                AudioServicesPlaySystemSound(SystemSoundID(kSystemSoundID_Vibrate))
                parent.onCodeScanned(stringValue)
            }
        }
    }
    
    func makeCoordinator() -> Coordinator {
        return Coordinator(parent: self)
    }
    
    func makeUIView(context: Context) -> UIView {
        let captureView = UIView(frame: .zero)
        
        let authStatus = AVCaptureDevice.authorizationStatus(for: .video)
        switch authStatus {
        case .authorized:
            setupCaptureSession(captureView, context: context)
        case .notDetermined:
            AVCaptureDevice.requestAccess(for: .video) { granted in
                if granted {
                    DispatchQueue.main.async {
                        setupCaptureSession(captureView, context: context)
                    }
                } else {
                    DispatchQueue.main.async {
                        self.errorMessage = "Camera access is required to scan QR codes."
                    }
                }
            }
        case .denied, .restricted:
            DispatchQueue.main.async {
                self.errorMessage = "Camera access is required to scan QR codes. Please enable it in Settings."
            }
        @unknown default:
            DispatchQueue.main.async {
                self.errorMessage = "Unknown camera authorization status."
            }
        }
        
        return captureView
    }
    
    private func setupCaptureSession(_ captureView: UIView, context: Context) {
        guard let captureDevice = AVCaptureDevice.default(for: .video) else {
            errorMessage = "Could not access camera."
            return
        }
        
        guard let captureDeviceInput = try? AVCaptureDeviceInput(device: captureDevice) else {
            errorMessage = "Could not create camera input."
            return
        }
        
        let captureSession = AVCaptureSession()
        
        // Set the preview layer
        let previewLayer = AVCaptureVideoPreviewLayer(session: captureSession)
        previewLayer.frame = captureView.layer.bounds
        previewLayer.videoGravity = .resizeAspectFill
        captureView.layer.addSublayer(previewLayer)
        
        // We'll handle frame updates in updateUIView instead of with notifications
        
        let metadataOutput = AVCaptureMetadataOutput()
        
        if captureSession.canAddInput(captureDeviceInput) && captureSession.canAddOutput(metadataOutput) {
            captureSession.addInput(captureDeviceInput)
            captureSession.addOutput(metadataOutput)
            
            metadataOutput.setMetadataObjectsDelegate(context.coordinator, queue: DispatchQueue.main)
            metadataOutput.metadataObjectTypes = [.qr]
            
            DispatchQueue.global(qos: .background).async {
                captureSession.startRunning()
            }
        } else {
            errorMessage = "Could not setup camera for QR scanning."
        }
    }
    
    func updateUIView(_ uiView: UIView, context: Context) {
        // Update the preview layer frame when the view size changes
        if let previewLayer = uiView.layer.sublayers?.first as? AVCaptureVideoPreviewLayer {
            previewLayer.frame = uiView.layer.bounds
        }
    }
}
