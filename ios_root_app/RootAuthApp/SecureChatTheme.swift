import SwiftUI

enum SecurePalette {
    static let backgroundTop = Color(red: 0.06, green: 0.10, blue: 0.16)
    static let backgroundBottom = Color(red: 0.03, green: 0.06, blue: 0.11)
    static let surface = Color(red: 0.10, green: 0.14, blue: 0.20).opacity(0.96)
    static let surfaceRaised = Color(red: 0.13, green: 0.18, blue: 0.25).opacity(0.98)
    static let border = Color.white.opacity(0.08)
    static let borderStrong = Color.white.opacity(0.14)
    static let textPrimary = Color(red: 0.93, green: 0.95, blue: 0.98)
    static let textSecondary = Color(red: 0.63, green: 0.70, blue: 0.80)
    static let textMuted = Color(red: 0.49, green: 0.57, blue: 0.66)
    static let accent = Color(red: 0.28, green: 0.62, blue: 0.98)
    static let accentSoft = Color(red: 0.18, green: 0.30, blue: 0.44)
    static let success = Color(red: 0.24, green: 0.80, blue: 0.54)
    static let warning = Color(red: 0.96, green: 0.71, blue: 0.29)
    static let danger = Color(red: 0.94, green: 0.39, blue: 0.43)
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
                .fill(SecurePalette.accent.opacity(0.16))
                .frame(width: 280, height: 280)
                .blur(radius: 80)
                .offset(x: 140, y: -180)
            Circle()
                .fill(SecurePalette.success.opacity(0.10))
                .frame(width: 220, height: 220)
                .blur(radius: 80)
                .offset(x: -160, y: 220)
        }
        .ignoresSafeArea()
    }
}

struct SecureCardModifier: ViewModifier {
    let padding: CGFloat

    func body(content: Content) -> some View {
        content
            .padding(padding)
            .background(
                RoundedRectangle(cornerRadius: 26, style: .continuous)
                    .fill(SecurePalette.surface)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 26, style: .continuous)
                    .stroke(SecurePalette.border, lineWidth: 1)
            )
            .shadow(color: Color.black.opacity(0.18), radius: 24, x: 0, y: 14)
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
    func secureCard(padding: CGFloat = 18) -> some View {
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
