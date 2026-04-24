import SwiftUI
import UIKit

enum SecurePalette {
    private static func uiColor(hex: UInt32, alpha: CGFloat = 1.0) -> UIColor {
        let red = CGFloat((hex >> 16) & 0xFF) / 255.0
        let green = CGFloat((hex >> 8) & 0xFF) / 255.0
        let blue = CGFloat(hex & 0xFF) / 255.0
        return UIColor(red: red, green: green, blue: blue, alpha: alpha)
    }

    private static func dynamicUIColor(light: UIColor, dark: UIColor) -> UIColor {
        UIColor { trait in
            trait.userInterfaceStyle == .dark ? dark : light
        }
    }

    static let backgroundTopUIColor = dynamicUIColor(
        light: uiColor(hex: 0xF2F2F7),
        dark: uiColor(hex: 0x0E1621)
    )
    static let backgroundBottomUIColor = dynamicUIColor(
        light: uiColor(hex: 0xE8EDF5),
        dark: uiColor(hex: 0x18222D)
    )

    static let backgroundTop = Color(uiColor: backgroundTopUIColor)
    static let backgroundBottom = Color(uiColor: backgroundBottomUIColor)
    static let groupedBackground = Color(uiColor: backgroundTopUIColor)
    static let surface = Color(uiColor: dynamicUIColor(
        light: .white,
        dark: uiColor(hex: 0x18222D)
    ))
    static let surfaceRaised = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0xFBFCFE),
        dark: uiColor(hex: 0x1E2A36)
    ))
    static let listSurface = Color(uiColor: dynamicUIColor(
        light: .white,
        dark: uiColor(hex: 0x18222D)
    ))
    static let groupedSurface = Color(uiColor: dynamicUIColor(
        light: .white,
        dark: uiColor(hex: 0x18222D)
    ))
    static let groupedSurfaceSubtle = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0xF7F8FB),
        dark: uiColor(hex: 0x21303D)
    ))
    static let groupedSurfaceElevated = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0xFFFFFF, alpha: 0.88),
        dark: uiColor(hex: 0x21303D, alpha: 0.94)
    ))
    static let glassToolbarSurface = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0xFFFFFF, alpha: 0.58),
        dark: uiColor(hex: 0x16202B, alpha: 0.72)
    ))
    static let selectedRow = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0x3390EC, alpha: 0.12),
        dark: uiColor(hex: 0x3390EC, alpha: 0.24)
    ))
    static let composerSurface = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0xFFFFFF, alpha: 0.76),
        dark: uiColor(hex: 0x18222D, alpha: 0.88)
    ))
    static let border = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0x203040, alpha: 0.10),
        dark: UIColor.white.withAlphaComponent(0.08)
    ))
    static let borderStrong = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0x203040, alpha: 0.16),
        dark: UIColor.white.withAlphaComponent(0.12)
    ))
    static let navigationSeparator = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0x000000, alpha: 0.05),
        dark: UIColor.white.withAlphaComponent(0.10)
    ))
    static let textPrimary = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0x111418),
        dark: uiColor(hex: 0xF1F4F8)
    ))
    static let textSecondary = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0x66758A),
        dark: uiColor(hex: 0x9AA7B5)
    ))
    static let textMuted = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0x7D8895),
        dark: uiColor(hex: 0x7D8895)
    ))
    static let accent = Color(uiColor: uiColor(hex: 0x3390EC))
    static let accentSky = Color(uiColor: uiColor(hex: 0x5FB3F4))
    static let accentMint = Color(uiColor: uiColor(hex: 0x35C59A))
    static let accentLavender = Color(uiColor: uiColor(hex: 0x7C7CF6))
    static let accentAmber = Color(uiColor: uiColor(hex: 0xF5A623))
    static let outgoingBubble = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0x3390EC),
        dark: uiColor(hex: 0x4A9FF3)
    ))
    static let accentSoft = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0x3390EC, alpha: 0.12),
        dark: uiColor(hex: 0x3390EC, alpha: 0.22)
    ))
    static let success = Color(uiColor: UIColor(red: 0.02, green: 0.59, blue: 0.41, alpha: 1.0))
    static let warning = Color(uiColor: UIColor(red: 0.85, green: 0.56, blue: 0.12, alpha: 1.0))
    static let danger = Color(uiColor: UIColor(red: 0.86, green: 0.15, blue: 0.15, alpha: 1.0))
    static let shadowAccent = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0x0E2137, alpha: 0.10),
        dark: UIColor.black.withAlphaComponent(0.26)
    ))
    static let flatCardShadow = Color(uiColor: dynamicUIColor(
        light: uiColor(hex: 0x0E2137, alpha: 0.04),
        dark: UIColor.black.withAlphaComponent(0.14)
    ))

    static func identityGradient(for seed: String) -> [Color] {
        let palette: [[Color]] = [
            [accent, accentSky],
            [accentLavender, accent],
            [accentMint, accentSky],
            [accentAmber, accent],
            [accentSky, accentLavender]
        ]
        let index = abs(seed.unicodeScalars.reduce(0) { $0 + Int($1.value) }) % palette.count
        return palette[index]
    }
}

enum SecureChatBubbleRole {
    case incoming
    case outgoing
}

struct SecureChatBubbleStyle {
    let role: SecureChatBubbleRole

    var fill: Color {
        switch role {
        case .incoming:
            return SecurePalette.groupedSurfaceElevated
        case .outgoing:
            return SecurePalette.outgoingBubble
        }
    }

    var stroke: Color {
        switch role {
        case .incoming:
            return SecurePalette.border
        case .outgoing:
            return Color.white.opacity(0.14)
        }
    }

    var primaryText: Color {
        switch role {
        case .incoming:
            return SecurePalette.textPrimary
        case .outgoing:
            return .white
        }
    }

    var secondaryText: Color {
        switch role {
        case .incoming:
            return SecurePalette.textSecondary
        case .outgoing:
            return Color.white.opacity(0.76)
        }
    }
}

enum SecureBannerTone: Equatable {
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

enum SecureIdentityKind {
    case person
    case group
    case device
    case system
}

enum SecurePresenceState {
    case secure
    case active
    case muted
    case review
    case none

    var tint: Color {
        switch self {
        case .secure:
            return SecurePalette.success
        case .active:
            return SecurePalette.accentSky
        case .muted:
            return SecurePalette.textMuted
        case .review:
            return SecurePalette.warning
        case .none:
            return Color.clear
        }
    }

    var symbol: String? {
        switch self {
        case .secure:
            return "checkmark"
        case .active:
            return "circle.fill"
        case .muted:
            return "bell.slash.fill"
        case .review:
            return "exclamationmark"
        case .none:
            return nil
        }
    }
}

enum SecureMediaKind: String {
    case none
    case file
    case photo
    case voice
    case link

    static func detect(in text: String) -> SecureMediaKind {
        let normalized = text.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        if normalized.hasPrefix("[file]") ||
            normalized.contains("upload") ||
            normalized.contains("bundle") ||
            normalized.contains("package") ||
            normalized.contains(".zip") ||
            normalized.contains(".pdf") {
            return .file
        }
        if normalized.hasPrefix("[photo]") ||
            normalized.contains("screenshot") ||
            normalized.contains("image") ||
            normalized.contains("photo") {
            return .photo
        }
        if normalized.hasPrefix("[voice]") ||
            normalized.contains("voice note") ||
            normalized.contains("audio") ||
            normalized.contains("recording") {
            return .voice
        }
        if normalized.contains("http://") ||
            normalized.contains("https://") ||
            normalized.contains("link") {
            return .link
        }
        return .none
    }

    var label: String {
        switch self {
        case .none:
            return ""
        case .file:
            return "File"
        case .photo:
            return "Photo"
        case .voice:
            return "Voice"
        case .link:
            return "Link"
        }
    }

    var icon: String {
        switch self {
        case .none:
            return "circle.fill"
        case .file:
            return "doc.fill"
        case .photo:
            return "photo.fill"
        case .voice:
            return "waveform"
        case .link:
            return "link"
        }
    }

    var accessorySymbol: String {
        switch self {
        case .none:
            return "circle.fill"
        case .file:
            return "arrow.down.circle.fill"
        case .photo:
            return "viewfinder.circle.fill"
        case .voice:
            return "play.circle.fill"
        case .link:
            return "arrow.up.right.circle.fill"
        }
    }

    var accent: Color {
        switch self {
        case .none:
            return SecurePalette.textMuted
        case .file:
            return SecurePalette.accent
        case .photo:
            return SecurePalette.accentSky
        case .voice:
            return SecurePalette.accentMint
        case .link:
            return SecurePalette.accentAmber
        }
    }
}

struct SecureSceneBackground: View {
    var body: some View {
        LinearGradient(
            colors: [SecurePalette.backgroundTop, SecurePalette.backgroundBottom],
            startPoint: .topLeading,
            endPoint: .bottomTrailing
        )
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
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .fill(SecurePalette.groupedSurfaceElevated)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .stroke(SecurePalette.border, lineWidth: 1)
            )
    }
}

struct SecureInputModifier: ViewModifier {
    func body(content: Content) -> some View {
        content
            .padding(.horizontal, 12)
            .padding(.vertical, 8)
            .background(
                RoundedRectangle(cornerRadius: 14, style: .continuous)
                    .fill(SecurePalette.surfaceRaised)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 14, style: .continuous)
                    .stroke(SecurePalette.borderStrong, lineWidth: 1)
            )
            .foregroundStyle(SecurePalette.textPrimary)
    }
}

struct SecureNavigationGlassModifier: ViewModifier {
    var cornerRadius: CGFloat

    func body(content: Content) -> some View {
        content
            .background(
                ZStack {
                    RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                        .fill(.ultraThinMaterial)
                    RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                        .fill(SecurePalette.glassToolbarSurface)
                }
            )
            .overlay(
                RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                    .stroke(SecurePalette.navigationSeparator, lineWidth: 0.5)
            )
            .shadow(color: SecurePalette.shadowAccent.opacity(0.35), radius: 10, x: 0, y: 3)
        }
}

private final class SecureNavigationBarSnapshot {
    let standardAppearance: UINavigationBarAppearance
    let scrollEdgeAppearance: UINavigationBarAppearance?
    let compactAppearance: UINavigationBarAppearance?
    let compactScrollEdgeAppearance: UINavigationBarAppearance?
    let tintColor: UIColor
    let prefersLargeTitles: Bool

    init(standardAppearance: UINavigationBarAppearance,
         scrollEdgeAppearance: UINavigationBarAppearance?,
         compactAppearance: UINavigationBarAppearance?,
         compactScrollEdgeAppearance: UINavigationBarAppearance?,
         tintColor: UIColor,
         prefersLargeTitles: Bool) {
        self.standardAppearance = standardAppearance
        self.scrollEdgeAppearance = scrollEdgeAppearance
        self.compactAppearance = compactAppearance
        self.compactScrollEdgeAppearance = compactScrollEdgeAppearance
        self.tintColor = tintColor
        self.prefersLargeTitles = prefersLargeTitles
    }
}

private final class SecureNavigationBarGlassCoordinator {
    var snapshot: SecureNavigationBarSnapshot?
    weak var navigationBar: UINavigationBar?
}

private struct SecureNavigationBarGlassConfigurator: UIViewRepresentable {
    func makeCoordinator() -> SecureNavigationBarGlassCoordinator {
        SecureNavigationBarGlassCoordinator()
    }

    func makeUIView(context: Context) -> UIView {
        let view = UIView(frame: .zero)
        view.isUserInteractionEnabled = false
        view.backgroundColor = .clear
        return view
    }

    func updateUIView(_ uiView: UIView, context: Context) {
        DispatchQueue.main.async {
            guard let navigationBar = resolveNavigationBar(from: uiView) else {
                return
            }

            if context.coordinator.navigationBar !== navigationBar {
                Self.restoreIfNeeded(using: context.coordinator)
                context.coordinator.snapshot = SecureNavigationBarSnapshot(
                    standardAppearance: navigationBar.standardAppearance.copy() as? UINavigationBarAppearance ?? navigationBar.standardAppearance,
                    scrollEdgeAppearance: navigationBar.scrollEdgeAppearance?.copy() as? UINavigationBarAppearance,
                    compactAppearance: navigationBar.compactAppearance?.copy() as? UINavigationBarAppearance,
                    compactScrollEdgeAppearance: navigationBar.compactScrollEdgeAppearance?.copy() as? UINavigationBarAppearance,
                    tintColor: navigationBar.tintColor,
                    prefersLargeTitles: navigationBar.prefersLargeTitles
                )
                context.coordinator.navigationBar = navigationBar
            }

            applyGlassAppearance(to: navigationBar)
        }
    }

    static func dismantleUIView(_ uiView: UIView, coordinator: SecureNavigationBarGlassCoordinator) {
        restoreIfNeeded(using: coordinator)
    }

    private func resolveNavigationBar(from view: UIView) -> UINavigationBar? {
        var responder: UIResponder? = view
        while let current = responder {
            if let navigationController = current as? UINavigationController {
                return navigationController.navigationBar
            }
            if let viewController = current as? UIViewController,
               let navigationBar = viewController.navigationController?.navigationBar {
                return navigationBar
            }
            responder = current.next
        }
        return nil
    }

    private func applyGlassAppearance(to navigationBar: UINavigationBar) {
        let appearance = UINavigationBarAppearance()
        appearance.configureWithTransparentBackground()
        appearance.backgroundEffect = UIBlurEffect(style: .systemUltraThinMaterial)
        appearance.backgroundColor = UIColor(SecurePalette.glassToolbarSurface).withAlphaComponent(0.86)
        appearance.shadowColor = UIColor { trait in
            trait.userInterfaceStyle == .dark
                ? UIColor.white.withAlphaComponent(0.10)
                : UIColor.black.withAlphaComponent(0.05)
        }
        appearance.titleTextAttributes = [
            .foregroundColor: UIColor.label
        ]
        appearance.largeTitleTextAttributes = [
            .foregroundColor: UIColor.label
        ]

        navigationBar.standardAppearance = appearance
        navigationBar.scrollEdgeAppearance = appearance
        navigationBar.compactAppearance = appearance
        if #available(iOS 15.0, *) {
            navigationBar.compactScrollEdgeAppearance = appearance
        }
        navigationBar.tintColor = UIColor(SecurePalette.accent)
        navigationBar.isTranslucent = true
    }

    private static func restoreIfNeeded(using coordinator: SecureNavigationBarGlassCoordinator) {
        guard let navigationBar = coordinator.navigationBar,
              let snapshot = coordinator.snapshot else {
            return
        }

        navigationBar.standardAppearance = snapshot.standardAppearance
        navigationBar.scrollEdgeAppearance = snapshot.scrollEdgeAppearance
        navigationBar.compactAppearance = snapshot.compactAppearance
        if #available(iOS 15.0, *) {
            navigationBar.compactScrollEdgeAppearance = snapshot.compactScrollEdgeAppearance
        }
        navigationBar.tintColor = snapshot.tintColor
        navigationBar.prefersLargeTitles = snapshot.prefersLargeTitles

        coordinator.snapshot = nil
        coordinator.navigationBar = nil
    }
}

struct SecureInsetGroupedSectionModifier: ViewModifier {
    var cornerRadius: CGFloat

    func body(content: Content) -> some View {
        content
            .background(
                RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                    .fill(SecurePalette.groupedSurface)
            )
            .overlay(
                RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                    .stroke(SecurePalette.border, lineWidth: 1)
            )
    }
}

struct SecureInsetGroupedListModifier: ViewModifier {
    func body(content: Content) -> some View {
        content
            .scrollContentBackground(.hidden)
            .background(SecurePalette.groupedBackground)
    }
}

struct SecureChatBubbleModifier: ViewModifier {
    let style: SecureChatBubbleStyle
    var cornerRadius: CGFloat
    var horizontalPadding: CGFloat
    var verticalPadding: CGFloat

    func body(content: Content) -> some View {
        content
            .padding(.horizontal, horizontalPadding)
            .padding(.vertical, verticalPadding)
            .background(
                RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                    .fill(style.fill)
            )
            .overlay(
                RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                    .stroke(style.stroke, lineWidth: 1)
            )
    }
}

struct SecureFloatingComposerModifier: ViewModifier {
    var cornerRadius: CGFloat

    func body(content: Content) -> some View {
        content
            .padding(.horizontal, 8)
            .padding(.vertical, 8)
            .background(
                ZStack {
                    RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                        .fill(.ultraThinMaterial)
                    RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                        .fill(SecurePalette.glassToolbarSurface.opacity(0.88))
                }
            )
            .overlay(
                RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                    .stroke(SecurePalette.borderStrong, lineWidth: 1)
            )
            .shadow(color: SecurePalette.shadowAccent, radius: 22, x: 0, y: 8)
    }
}

extension View {
    func secureCard(padding: CGFloat = 16) -> some View {
        modifier(SecureCardModifier(padding: padding))
    }

    func secureInput() -> some View {
        modifier(SecureInputModifier())
    }

    func secureNavigationGlass(cornerRadius: CGFloat = 18) -> some View {
        modifier(SecureNavigationGlassModifier(cornerRadius: cornerRadius))
    }

    func secureInsetGroupedSection(cornerRadius: CGFloat = 18) -> some View {
        modifier(SecureInsetGroupedSectionModifier(cornerRadius: cornerRadius))
    }

    func secureInsetGroupedList() -> some View {
        modifier(SecureInsetGroupedListModifier())
    }

    func secureChatBubble(_ style: SecureChatBubbleStyle,
                          cornerRadius: CGFloat = 18,
                          horizontalPadding: CGFloat = 12,
                          verticalPadding: CGFloat = 8) -> some View {
        modifier(
            SecureChatBubbleModifier(
                style: style,
                cornerRadius: cornerRadius,
                horizontalPadding: horizontalPadding,
                verticalPadding: verticalPadding
            )
        )
    }

    func secureFloatingComposer(cornerRadius: CGFloat = 26) -> some View {
        modifier(SecureFloatingComposerModifier(cornerRadius: cornerRadius))
    }

    func secureGlassNavigationBar() -> some View {
        background(SecureNavigationBarGlassConfigurator())
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
                        .fixedSize(horizontal: false, vertical: true)
                }
            }

            Spacer(minLength: 0)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 9)
        .background(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .fill(tone.fill)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .stroke(tone.accent.opacity(0.35), lineWidth: 1)
        )
    }
}

struct SecureSectionHeader: View {
    let eyebrow: String
    let title: String
    let detail: String

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            if !eyebrow.isEmpty {
                Text(eyebrow.uppercased())
                    .font(.caption.weight(.semibold))
                    .tracking(1.2)
                    .foregroundStyle(SecurePalette.textMuted)
            }
            Text(title)
                .font(.headline.weight(.semibold))
                .foregroundStyle(SecurePalette.textPrimary)
            if !detail.isEmpty {
                Text(detail)
                    .font(.footnote)
                    .foregroundStyle(SecurePalette.textSecondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
    }
}

struct SecureMetricBadge: View {
    let title: String
    let value: String
    let systemImage: String
    var accent: Color = SecurePalette.accent

    var body: some View {
        HStack(spacing: 8) {
            Image(systemName: systemImage)
                .font(.system(size: 11, weight: .semibold))
                .foregroundStyle(accent)
                .frame(width: 22, height: 22)
                .background(
                    Circle()
                        .fill(accent.opacity(0.14))
                )
            Text(value)
                .font(.caption.weight(.semibold))
                .foregroundStyle(SecurePalette.textPrimary)
                .lineLimit(1)
                .minimumScaleFactor(0.78)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 8)
        .background(
            Capsule()
                .fill(SecurePalette.surfaceRaised)
        )
        .overlay(
            Capsule()
                .stroke(SecurePalette.border, lineWidth: 1)
        )
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(Text("\(title): \(value)"))
    }
}

struct SecureMetricTile: View {
    let label: String
    let value: String
    let icon: String
    var monospaced: Bool = false
    var accent: Color = SecurePalette.accent

    var body: some View {
        HStack(alignment: .top, spacing: 12) {
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .fill(accent.opacity(0.12))
                .frame(width: 36, height: 36)
                .overlay(
                    Image(systemName: icon)
                        .font(.system(size: 14, weight: .semibold))
                        .foregroundStyle(accent)
                )

            VStack(alignment: .leading, spacing: 8) {
                Text(value)
                    .font(monospaced
                          ? .system(.title3, design: .monospaced).weight(.semibold)
                          : .headline.weight(.semibold))
                    .foregroundStyle(SecurePalette.textPrimary)
                    .lineLimit(2)
                    .minimumScaleFactor(0.75)
                if !label.isEmpty {
                    Text(label)
                        .font(.caption2.weight(.semibold))
                        .foregroundStyle(SecurePalette.textMuted)
                        .lineLimit(1)
                        .minimumScaleFactor(0.82)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .fill(SecurePalette.surfaceRaised)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .stroke(SecurePalette.border, lineWidth: 1)
        )
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(Text("\(label): \(value)"))
    }
}

struct SecureMediaHintPill: View {
    let kind: SecureMediaKind

    var body: some View {
        if kind != .none {
            Image(systemName: kind.icon)
                .font(.system(size: 10, weight: .bold))
                .foregroundStyle(kind.accent)
                .frame(width: 22, height: 22)
                .background(
                    Circle()
                        .fill(kind.accent.opacity(0.12))
                )
                .overlay(
                    Circle()
                        .stroke(kind.accent.opacity(0.18), lineWidth: 1)
                )
        }
    }
}

struct SecureMediaPreviewCard: View {
    let kind: SecureMediaKind
    let title: String
    let detail: String
    var outgoing: Bool = false

    var body: some View {
        HStack(spacing: 10) {
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .fill((outgoing ? Color.white.opacity(0.15) : kind.accent.opacity(0.14)))
                .frame(width: 36, height: 36)
                .overlay(
                    Image(systemName: kind.icon)
                        .font(.system(size: 14, weight: .semibold))
                        .foregroundStyle(outgoing ? Color.white : kind.accent)
                )

            VStack(alignment: .leading, spacing: 3) {
                HStack(spacing: 6) {
                    Text(kind.label.uppercased())
                        .font(.caption2.weight(.bold))
                        .tracking(0.6)
                        .foregroundStyle(outgoing ? Color.white.opacity(0.72) : kind.accent)
                    Text(title)
                        .font(.footnote.weight(.semibold))
                        .foregroundStyle(outgoing ? Color.white : SecurePalette.textPrimary)
                        .lineLimit(1)
                }
                Text(detail)
                    .font(.caption)
                    .foregroundStyle(outgoing ? Color.white.opacity(0.74) : SecurePalette.textSecondary)
                    .lineLimit(2)
            }
            Spacer(minLength: 0)

            Image(systemName: kind.accessorySymbol)
                .font(.system(size: 15, weight: .semibold))
                .foregroundStyle(outgoing ? Color.white.opacity(0.82) : kind.accent)
        }
        .padding(12)
        .background(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .fill(outgoing ? Color.white.opacity(0.15) : SecurePalette.groupedSurfaceSubtle)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .stroke(outgoing ? Color.white.opacity(0.12) : SecurePalette.border, lineWidth: 1)
        )
    }
}

struct SecureEmptyStateIllustration: View {
    let systemImage: String
    var accent: Color = SecurePalette.accent
    var size: CGFloat = 96

    var body: some View {
        ZStack {
            Circle()
                .fill(accent.opacity(0.12))
                .frame(width: size, height: size)

            Circle()
                .fill(SecurePalette.accentSky.opacity(0.12))
                .frame(width: size * 0.62, height: size * 0.62)
                .offset(x: size * 0.25, y: size * 0.18)

            Circle()
                .fill(SecurePalette.surfaceRaised)
                .frame(width: size * 0.68, height: size * 0.68)
                .overlay(
                    Image(systemName: systemImage)
                        .font(.system(size: size * 0.24, weight: .semibold))
                        .foregroundStyle(accent)
                )
                .overlay(
                    Circle()
                        .stroke(SecurePalette.border, lineWidth: 1)
                )
        }
        .frame(width: size + 24, height: size + 24)
    }
}

struct SecureIdentityAvatar: View {
    let title: String
    let seed: String
    let kind: SecureIdentityKind
    var size: CGFloat = 44
    var presence: SecurePresenceState = .none
    var prominent: Bool = false

    private var initials: String {
        let tokens = title
            .split(whereSeparator: { $0.isWhitespace || $0 == "-" || $0 == "_" })
            .prefix(2)
            .map { String($0.prefix(1)).uppercased() }
        let joined = tokens.joined()
        return joined.isEmpty ? "#" : joined
    }

    private var gradient: LinearGradient {
        LinearGradient(
            colors: SecurePalette.identityGradient(for: seed),
            startPoint: .topLeading,
            endPoint: .bottomTrailing
        )
    }

    private var iconName: String {
        switch kind {
        case .person:
            return "person.fill"
        case .group:
            return "person.3.fill"
        case .device:
            return "iphone.gen3"
        case .system:
            return "lock.shield.fill"
        }
    }

    var body: some View {
        ZStack(alignment: .bottomTrailing) {
            ZStack {
                switch kind {
                case .person:
                    Circle()
                        .fill(gradient)
                        .overlay(
                            Circle()
                                .fill(.white.opacity(0.10))
                                .frame(width: size * 0.42, height: size * 0.42)
                                .offset(x: size * 0.18, y: -size * 0.18)
                        )
                    Text(initials)
                        .font(.system(size: max(size * 0.30, 11), weight: .bold, design: .rounded))
                        .foregroundStyle(Color.white)
                case .group:
                    RoundedRectangle(cornerRadius: size * 0.30, style: .continuous)
                        .fill(gradient)
                    HStack(spacing: size * 0.04) {
                        Circle()
                            .fill(Color.white.opacity(0.86))
                            .frame(width: size * 0.28, height: size * 0.28)
                        Circle()
                            .fill(Color.white.opacity(0.52))
                            .frame(width: size * 0.22, height: size * 0.22)
                        Circle()
                            .fill(Color.white.opacity(0.32))
                            .frame(width: size * 0.18, height: size * 0.18)
                    }
                    .offset(y: size * 0.02)
                case .device, .system:
                    RoundedRectangle(cornerRadius: size * 0.30, style: .continuous)
                        .fill(gradient)
                    Image(systemName: iconName)
                        .font(.system(size: size * 0.36, weight: .semibold))
                        .foregroundStyle(Color.white)
                }
            }
            .frame(width: size, height: size)
            .overlay(
                RoundedRectangle(cornerRadius: kind == .person ? size / 2 : size * 0.30, style: .continuous)
                    .stroke(Color.white.opacity(0.16), lineWidth: 1)
                    .opacity(kind == .person ? 0 : 1)
            )
            .overlay(
                Circle()
                    .stroke(SecurePalette.border.opacity(prominent ? 0.0 : 0.35), lineWidth: prominent ? 0 : 1)
                    .opacity(kind == .person ? 1 : 0)
            )
            .shadow(color: prominent ? SecurePalette.accent.opacity(0.16) : .clear,
                    radius: prominent ? 14 : 0,
                    x: 0,
                    y: prominent ? 8 : 0)

            if presence != .none, let symbol = presence.symbol {
                Circle()
                    .fill(presence.tint)
                    .frame(width: max(size * 0.30, 12), height: max(size * 0.30, 12))
                    .overlay(
                        Image(systemName: symbol)
                            .font(.system(size: max(size * 0.12, 6), weight: .bold))
                            .foregroundStyle(Color.white)
                    )
                    .overlay(
                        Circle()
                            .stroke(SecurePalette.listSurface, lineWidth: 2)
                    )
            }
        }
        .frame(width: size, height: size)
    }
}

struct SecurePrimaryButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.subheadline.weight(.semibold))
            .foregroundStyle(Color.white)
            .padding(.horizontal, 18)
            .padding(.vertical, 10)
            .frame(maxWidth: .infinity)
            .background(
                RoundedRectangle(cornerRadius: 14, style: .continuous)
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
            .padding(.vertical, 9)
            .frame(maxWidth: .infinity)
            .background(
                RoundedRectangle(cornerRadius: 14, style: .continuous)
                    .fill(configuration.isPressed ? SecurePalette.surfaceRaised.opacity(0.85) : SecurePalette.surfaceRaised)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 14, style: .continuous)
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
