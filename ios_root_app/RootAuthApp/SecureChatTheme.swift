import SwiftUI
import UIKit

enum SecurePalette {
    private static func dynamicUIColor(light: UIColor, dark: UIColor) -> UIColor {
        UIColor { trait in
            trait.userInterfaceStyle == .dark ? dark : light
        }
    }

    static let backgroundTopUIColor = dynamicUIColor(
        light: UIColor(red: 0.95, green: 0.97, blue: 1.00, alpha: 1.0),
        dark: UIColor(red: 0.10, green: 0.14, blue: 0.22, alpha: 1.0)
    )
    static let backgroundBottomUIColor = dynamicUIColor(
        light: UIColor(red: 0.91, green: 0.94, blue: 0.99, alpha: 1.0),
        dark: UIColor(red: 0.07, green: 0.10, blue: 0.17, alpha: 1.0)
    )

    static let backgroundTop = Color(uiColor: backgroundTopUIColor)
    static let backgroundBottom = Color(uiColor: backgroundBottomUIColor)
    static let surface = Color(uiColor: dynamicUIColor(
        light: UIColor.white.withAlphaComponent(0.84),
        dark: UIColor.secondarySystemBackground.withAlphaComponent(0.88)
    ))
    static let surfaceRaised = Color(uiColor: dynamicUIColor(
        light: UIColor.white.withAlphaComponent(0.94),
        dark: UIColor.tertiarySystemBackground.withAlphaComponent(0.90)
    ))
    static let border = Color(uiColor: dynamicUIColor(
        light: UIColor.separator.withAlphaComponent(0.40),
        dark: UIColor.separator.withAlphaComponent(0.65)
    ))
    static let borderStrong = Color(uiColor: dynamicUIColor(
        light: UIColor.separator.withAlphaComponent(0.62),
        dark: UIColor.separator.withAlphaComponent(0.82)
    ))
    static let textPrimary = Color(uiColor: .label)
    static let textSecondary = Color(uiColor: .secondaryLabel)
    static let textMuted = Color(uiColor: .tertiaryLabel)
    static let accent = Color(uiColor: .systemBlue)
    static let accentSoft = Color(uiColor: dynamicUIColor(
        light: UIColor.systemBlue.withAlphaComponent(0.16),
        dark: UIColor.systemBlue.withAlphaComponent(0.24)
    ))
    static let success = Color(uiColor: .systemGreen)
    static let warning = Color(uiColor: .systemOrange)
    static let danger = Color(uiColor: .systemRed)
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
                .fill(SecurePalette.accent.opacity(0.10))
                .frame(width: 260, height: 260)
                .blur(radius: 90)
                .offset(x: 120, y: -170)
            Circle()
                .fill(SecurePalette.success.opacity(0.07))
                .frame(width: 190, height: 190)
                .blur(radius: 85)
                .offset(x: -130, y: 200)
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
                RoundedRectangle(cornerRadius: 22, style: .continuous)
                    .fill(SecurePalette.surface)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 22, style: .continuous)
                    .stroke(SecurePalette.border, lineWidth: 1)
            )
            .shadow(color: Color.black.opacity(0.06), radius: 8, x: 0, y: 4)
    }
}

struct SecureInputModifier: ViewModifier {
    func body(content: Content) -> some View {
        content
            .padding(.horizontal, 14)
            .padding(.vertical, 12)
            .background(
                RoundedRectangle(cornerRadius: 18, style: .continuous)
                    .fill(SecurePalette.surfaceRaised)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 18, style: .continuous)
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
                .font(.system(size: 15, weight: .semibold))
                .foregroundStyle(tone.accent)
                .frame(width: 28, height: 28)
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
        .padding(14)
        .background(
            RoundedRectangle(cornerRadius: 18, style: .continuous)
                .fill(tone.fill)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 18, style: .continuous)
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
        .padding(14)
        .background(
            RoundedRectangle(cornerRadius: 20, style: .continuous)
                .fill(SecurePalette.surfaceRaised)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 20, style: .continuous)
                .stroke(SecurePalette.border, lineWidth: 1)
        )
    }
}

struct SecurePrimaryButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.headline.weight(.semibold))
            .foregroundStyle(Color.white)
            .padding(.horizontal, 18)
            .padding(.vertical, 14)
            .frame(maxWidth: .infinity)
            .background(
                RoundedRectangle(cornerRadius: 18, style: .continuous)
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
            .padding(.vertical, 12)
            .frame(maxWidth: .infinity)
            .background(
                RoundedRectangle(cornerRadius: 18, style: .continuous)
                    .fill(configuration.isPressed ? SecurePalette.surfaceRaised.opacity(0.85) : SecurePalette.surfaceRaised)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 18, style: .continuous)
                    .stroke(SecurePalette.borderStrong, lineWidth: 1)
            )
            .scaleEffect(configuration.isPressed ? 0.985 : 1.0)
            .animation(.easeOut(duration: 0.16), value: configuration.isPressed)
    }
}
