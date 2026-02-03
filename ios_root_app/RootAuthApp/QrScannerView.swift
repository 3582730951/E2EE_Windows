import SwiftUI
import AVFoundation

struct QrScannerView: UIViewRepresentable {
    let onCode: (String) -> Void

    func makeUIView(context: Context) -> PreviewView {
        let view = PreviewView()
        context.coordinator.configureSession(view: view)
        return view
    }

    func updateUIView(_ uiView: PreviewView, context: Context) {
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(onCode: onCode)
    }

    final class Coordinator: NSObject, AVCaptureMetadataOutputObjectsDelegate {
        private let onCode: (String) -> Void
        private var session: AVCaptureSession?
        private var didScan = false

        init(onCode: @escaping (String) -> Void) {
            self.onCode = onCode
        }

        func configureSession(view: PreviewView) {
            let session = AVCaptureSession()
            guard let device = AVCaptureDevice.default(for: .video) else { return }
            guard let input = try? AVCaptureDeviceInput(device: device) else { return }
            if session.canAddInput(input) {
                session.addInput(input)
            }
            let output = AVCaptureMetadataOutput()
            if session.canAddOutput(output) {
                session.addOutput(output)
                output.setMetadataObjectsDelegate(self, queue: DispatchQueue.main)
                output.metadataObjectTypes = [.qr]
            }
            view.videoPreviewLayer.session = session
            view.videoPreviewLayer.videoGravity = .resizeAspectFill
            session.startRunning()
            self.session = session
        }

        func metadataOutput(_ output: AVCaptureMetadataOutput,
                            didOutput metadataObjects: [AVMetadataObject],
                            from connection: AVCaptureConnection) {
            guard !didScan else { return }
            if let obj = metadataObjects.first as? AVMetadataMachineReadableCodeObject,
               obj.type == .qr,
               let value = obj.stringValue {
                didScan = true
                onCode(value)
            }
        }
    }
}

final class PreviewView: UIView {
    override class var layerClass: AnyClass {
        AVCaptureVideoPreviewLayer.self
    }

    var videoPreviewLayer: AVCaptureVideoPreviewLayer {
        layer as! AVCaptureVideoPreviewLayer
    }
}
