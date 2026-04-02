import SwiftUI
import UIKit

enum SecurePalette {
    private static func dynamicUIColor(light: UIColor, dark: UIColor) -> UIColor {
        UIColor { trait in
            trait.userInterfaceStyle == .dark ? dark : light
        }
    }

    static let backgroundTopUIColor = dynamicUIColor(
        light: UIColor(red: 0.97, green: 0.98, blue: 0.99, alpha: 1.0),
        dark: UIColor(red: 0.09, green: 0.13, blue: 0.18, alpha: 1.0)
    )
    static let backgroundBottomUIColor = dynamicUIColor(
        light: UIColor(red: 0.94, green: 0.96, blue: 0.99, alpha: 1.0),
        dark: UIColor(red: 0.06, green: 0.10, blue: 0.16, alpha: 1.0)
    )

    static let backgroundTop = Color(uiColor: backgroundTopUIColor)
    static let backgroundBottom = Color(uiColor: backgroundBottomUIColor)
    static let surface = Color(uiColor: dynamicUIColor(
        light: UIColor(red: 1.0, green: 1.0, blue: 1.0, alpha: 0.94),
        dark: UIColor(red: 0.07, green: 0.10, blue: 0.15, alpha: 0.94)
    ))
    static let surfaceRaised = Color(uiColor: dynamicUIColor(
        light: UIColor(red: 0.95, green: 0.97, blue: 0.99, alpha: 0.98),
        dark: UIColor(red: 0.10, green: 0.14, blue: 0.19, alpha: 0.98)
    ))
    static let border = Color(uiColor: dynamicUIColor(
        light: UIColor(red: 0.89, green: 0.93, blue: 0.99, alpha: 1.0),
        dark: UIColor.white.withAlphaComponent(0.08)
    ))
    static let borderStrong = Color(uiColor: dynamicUIColor(
        light: UIColor(red: 0.82, green: 0.88, blue: 0.97, alpha: 1.0),
        dark: UIColor.white.withAlphaComponent(0.14)
    ))
    static let textPrimary = Color(uiColor: dynamicUIColor(
        light: UIColor(red: 0.06, green: 0.09, blue: 0.16, alpha: 1.0),
        dark: UIColor(red: 0.91, green: 0.93, blue: 0.96, alpha: 1.0)
    ))
    static let textSecondary = Color(uiColor: dynamicUIColor(
        light: UIColor(red: 0.39, green: 0.47, blue: 0.58, alpha: 1.0),
        dark: UIColor(red: 0.62, green: 0.68, blue: 0.74, alpha: 1.0)
    ))
    static let textMuted = Color(uiColor: dynamicUIColor(
        light: UIColor(red: 0.48, green: 0.56, blue: 0.66, alpha: 1.0),
        dark: UIColor(red: 0.51, green: 0.57, blue: 0.64, alpha: 1.0)
    ))
    static let accent = Color(uiColor: UIColor(red: 0.15, green: 0.39, blue: 0.92, alpha: 1.0))
    static let accentSoft = Color(uiColor: dynamicUIColor(
        light: UIColor(red: 0.15, green: 0.39, blue: 0.92, alpha: 0.12),
        dark: UIColor(red: 0.15, green: 0.39, blue: 0.92, alpha: 0.22)
    ))
    static let success = Color(uiColor: UIColor(red: 0.02, green: 0.59, blue: 0.41, alpha: 1.0))
    static let warning = Color(uiColor: UIColor(red: 0.85, green: 0.56, blue: 0.12, alpha: 1.0))
    static let danger = Color(uiColor: UIColor(red: 0.86, green: 0.15, blue: 0.15, alpha: 1.0))
}

enum SecureBannerTone {
    case neutral
    case success
    case warning
    case danger

    var accent: Color {
        switch self {
        case .neutral:
            return SecurePalette.accent
        case .success:
            return SecurePalette.success
        case .warning:
            return SecurePalette.warning
        case .danger:
            return SecurePalette.danger
        }
    }

    var fill: Color {
        accent.opacity(0.16)
    }
}

struct SecureSceneBackground: View {
    var body: some View {
        ZStack {
            LinearGradient(
                colors: [SecurePalette.backgroundTop, SecurePalette.backgroundBottom],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
            Circle()
                .fill(SecurePalette.accent.opacity(0.08))
                .frame(width: 220, height: 220)
                .blur(radius: 84)
                .offset(x: 120, y: -180)
            Circle()
                .fill(SecurePalette.success.opacity(0.05))
                .frame(width: 160, height: 160)
                .blur(radius: 72)
                .offset(x: -120, y: 210)
        }
        .ignoresSafeArea()
    }
}

private final class SecureHostBackgroundView: UIView {
    override class var layerClass: AnyClass {
        CAGradientLayer.self
    }

    private var gradientLayer: CAGradientLayer {
        layer as! CAGradientLayer
    }

    override init(frame: CGRect) {
        super.init(frame: frame)
        isUserInteractionEnabled = false
        autoresizingMask = [.flexibleWidth, .flexibleHeight]
        backgroundColor = .clear
        clipsToBounds = true
        configureGradient()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func configureGradient() {
        let resolvedTop = SecurePalette.backgroundTopUIColor.resolvedColor(with: traitCollection)
        let resolvedBottom = SecurePalette.backgroundBottomUIColor.resolvedColor(with: traitCollection)
        gradientLayer.colors = [
            resolvedTop.cgColor,
            resolvedBottom.cgColor
        ]
        gradientLayer.locations = [0.0, 1.0]
        gradientLayer.startPoint = CGPoint(x: 0.1, y: 0.0)
        gradientLayer.endPoint = CGPoint(x: 0.9, y: 1.0)
    }

    override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
        super.traitCollectionDidChange(previousTraitCollection)
        configureGradient()
    }
}

private final class SecureWindowProbeView: UIView {
    var onUpdate: ((UIView) -> Void)?

    override func didMoveToWindow() {
        super.didMoveToWindow()
        onUpdate?(self)
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.onUpdate?(self)
        }
    }

    override func didMoveToSuperview() {
        super.didMoveToSuperview()
        onUpdate?(self)
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        onUpdate?(self)
    }
}

struct SecureWindowConfigurator: UIViewRepresentable {
    private static let hostBackgroundTag = 0xE2EEB001

    private func installHostBackground(in rootView: UIView) {
        let backgroundView: SecureHostBackgroundView
        if let existing = rootView.viewWithTag(Self.hostBackgroundTag) as? SecureHostBackgroundView {
            backgroundView = existing
        } else {
            backgroundView = SecureHostBackgroundView(frame: rootView.bounds)
            backgroundView.tag = Self.hostBackgroundTag
            rootView.insertSubview(backgroundView, at: 0)
        }

        backgroundView.frame = rootView.bounds
        rootView.sendSubviewToBack(backgroundView)
    }

    private func clearHostChain(from view: UIView?) {
        var current = view
        while let node = current {
            node.backgroundColor = .clear
            current = node.superview
        }
    }

    private func applyBackground(from view: UIView?) {
        let fill = UIColor(SecurePalette.backgroundBottom)
        clearHostChain(from: view)

        if let window = view?.window {
            window.backgroundColor = fill
            window.rootViewController?.view.backgroundColor = .clear
            if let rootView = window.rootViewController?.view {
                installHostBackground(in: rootView)
            }
        }
    }

    func makeUIView(context: Context) -> UIView {
        let view = SecureWindowProbeView(frame: .zero)
        view.onUpdate = { probe in
            applyBackground(from: probe)
        }
        view.backgroundColor = .clear
        view.isUserInteractionEnabled = false
        DispatchQueue.main.async {
            applyBackground(from: view)
        }
        return view
    }

    func updateUIView(_ uiView: UIView, context: Context) {
        uiView.backgroundColor = .clear
        DispatchQueue.main.async {
            applyBackground(from: uiView)
        }
    }
}

struct SecureCardModifier: ViewModifier {
    let padding: CGFloat

    func body(content: Content) -> some View {
        content
            .padding(padding)
            .background(
                RoundedRectangle(cornerRadius: 20, style: .continuous)
                    .fill(SecurePalette.surface)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 20, style: .continuous)
                    .stroke(SecurePalette.border, lineWidth: 1)
            )
            .shadow(color: Color.black.opacity(0.04), radius: 5, x: 0, y: 2)
    }
}

struct SecureInputModifier: ViewModifier {
    func body(content: Content) -> some View {
        content
            .padding(.horizontal, 12)
            .padding(.vertical, 10)
            .background(
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .fill(SecurePalette.surfaceRaised)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .stroke(SecurePalette.borderStrong, lineWidth: 1)
            )
            .foregroundStyle(SecurePalette.textPrimary)
    }
}

extension View {
    func secureCard(padding: CGFloat = 16) -> some View {
        modifier(SecureCardModifier(padding: padding))
    }

    func secureInput() -> some View {
        modifier(SecureInputModifier())
    }
}

struct SecureStatusBanner: View {
    let title: String
    let detail: String?
    let tone: SecureBannerTone
    let systemImage: String

    var body: some View {
        HStack(alignment: .top, spacing: 12) {
            Image(systemName: systemImage)
                .font(.system(size: 14, weight: .semibold))
                .foregroundStyle(tone.accent)
                .frame(width: 24, height: 24)
                .background(
                    Circle()
                        .fill(tone.accent.opacity(0.18))
                )

            VStack(alignment: .leading, spacing: 4) {
                Text(title)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(SecurePalette.textPrimary)
                if let detail, !detail.isEmpty {
                    Text(detail)
                        .font(.footnote)
                        .foregroundStyle(SecurePalette.textSecondary)
                }
            }

            Spacer(minLength: 0)
        }
        .padding(12)
        .background(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .fill(tone.fill)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .stroke(tone.accent.opacity(0.35), lineWidth: 1)
        )
    }
}

struct SecureSectionHeader: View {
    let eyebrow: String
    let title: String
    let detail: String

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(eyebrow.uppercased())
                .font(.caption.weight(.semibold))
                .tracking(1.2)
                .foregroundStyle(SecurePalette.textMuted)
            Text(title)
                .font(.title3.weight(.semibold))
                .foregroundStyle(SecurePalette.textPrimary)
            Text(detail)
                .font(.footnote)
                .foregroundStyle(SecurePalette.textSecondary)
        }
    }
}

struct SecureMetricTile: View {
    let label: String
    let value: String
    let icon: String
    var monospaced: Bool = false

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(spacing: 8) {
                Image(systemName: icon)
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(SecurePalette.accent)
                Text(label.uppercased())
                    .font(.caption2.weight(.semibold))
                    .tracking(1.1)
                    .foregroundStyle(SecurePalette.textMuted)
            }

            Text(value)
                .font(monospaced
                      ? .system(.title3, design: .monospaced).weight(.semibold)
                      : .headline.weight(.semibold))
                .foregroundStyle(SecurePalette.textPrimary)
                .lineLimit(2)
                .minimumScaleFactor(0.75)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .fill(SecurePalette.surfaceRaised)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .stroke(SecurePalette.border, lineWidth: 1)
        )
    }
}

struct SecurePrimaryButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.subheadline.weight(.semibold))
            .foregroundStyle(Color.white)
            .padding(.horizontal, 18)
            .padding(.vertical, 12)
            .frame(maxWidth: .infinity)
            .background(
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .fill(configuration.isPressed ? SecurePalette.accent.opacity(0.85) : SecurePalette.accent)
            )
            .scaleEffect(configuration.isPressed ? 0.98 : 1.0)
            .animation(.easeOut(duration: 0.16), value: configuration.isPressed)
    }
}

struct SecureSecondaryButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.subheadline.weight(.semibold))
            .foregroundStyle(SecurePalette.textPrimary)
            .padding(.horizontal, 16)
            .padding(.vertical, 11)
            .frame(maxWidth: .infinity)
            .background(
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .fill(configuration.isPressed ? SecurePalette.surfaceRaised.opacity(0.85) : SecurePalette.surfaceRaised)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .stroke(SecurePalette.borderStrong, lineWidth: 1)
            )
            .scaleEffect(configuration.isPressed ? 0.985 : 1.0)
            .animation(.easeOut(duration: 0.16), value: configuration.isPressed)
    }
}

struct SecureCircularIconButton: View {
    let systemImage: String
    let accessibilityLabel: String
    var iconSize: CGFloat = 15
    var buttonSize: CGFloat = 44
    var foreground: Color = SecurePalette.textPrimary
    var fill: Color = SecurePalette.surfaceRaised
    var stroke: Color = SecurePalette.borderStrong
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Image(systemName: systemImage)
                .font(.system(size: iconSize, weight: .semibold))
                .foregroundStyle(foreground)
                .frame(width: buttonSize, height: buttonSize)
                .background(
                    Circle()
                        .fill(fill)
                )
                .overlay(
                    Circle()
                        .stroke(stroke, lineWidth: 1)
                )
        }
        .buttonStyle(.plain)
        .contentShape(Circle())
        .accessibilityLabel(accessibilityLabel)
    }
}
