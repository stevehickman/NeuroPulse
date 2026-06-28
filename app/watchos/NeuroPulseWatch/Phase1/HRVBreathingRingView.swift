import SwiftUI

// Phase 1 — HRV breathing ring.
// Expanding ring = inhale, contracting ring = exhale.
// Driven by the pacer phase and elapsed-percent values from the hub
// (np_hrv_pacer firmware module, NP-FW-HRV-001 §7).

struct HRVBreathingRingView: View {

    let phase: PacerPhase
    let elapsedPercent: UInt8       // 0–100, fraction of current phase elapsed
    let coherenceScore: Float?      // 0.0–10.0, nil when no HRV session

    // Ring scales from minScale (exhale complete) to maxScale (inhale complete).
    private let minScale: CGFloat = 0.45
    private let maxScale: CGFloat = 0.90

    private var scale: CGFloat {
        let t = CGFloat(elapsedPercent) / 100.0
        switch phase {
        case .inhale: return minScale + (maxScale - minScale) * t
        case .exhale: return maxScale - (maxScale - minScale) * t
        }
    }

    private var ringColor: Color {
        guard let score = coherenceScore else { return .gray }
        switch score {
        case 0..<4:  return .red
        case 4..<6:  return .orange
        case 6..<8:  return .yellow
        default:     return .green
        }
    }

    private var phaseLabel: String {
        switch phase {
        case .inhale: return "Inhale"
        case .exhale: return "Exhale"
        }
    }

    var body: some View {
        ZStack {
            // Outer static reference ring.
            Circle()
                .stroke(Color.white.opacity(0.15), lineWidth: 2)
                .frame(width: 120, height: 120)

            // Animated breathing ring.
            Circle()
                .stroke(ringColor, lineWidth: 6)
                .frame(width: 120, height: 120)
                .scaleEffect(scale)
                .animation(.linear(duration: 0.1), value: scale)

            VStack(spacing: 2) {
                Text(phaseLabel)
                    .font(.system(size: 11, weight: .medium))
                    .foregroundColor(.white.opacity(0.8))

                if let score = coherenceScore {
                    Text(String(format: "%.1f", score))
                        .font(.system(size: 22, weight: .bold, design: .rounded))
                        .foregroundColor(ringColor)
                    Text("coherence")
                        .font(.system(size: 9))
                        .foregroundColor(.white.opacity(0.5))
                } else {
                    Text("–")
                        .font(.system(size: 22, weight: .bold, design: .rounded))
                        .foregroundColor(.gray)
                }
            }
        }
        .frame(width: 130, height: 130)
    }
}

#Preview {
    HRVBreathingRingView(phase: .inhale, elapsedPercent: 60, coherenceScore: 7.4)
        .background(Color.black)
}
