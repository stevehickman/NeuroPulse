package life.neurone.shared.ui

// How each socket state looks (NP-SIM-FW-001 §25). Pure data, so a test can hold the rule that matters: no two states may differ
// by colour alone. Colour is a second cue on top of an outline and a marker, because a person who cannot tell amber from red
// must still be able to tell commanded from driven.

enum class TileOutline { NONE, THIN, DOTTED, THICK }

/** The glyph drawn in the middle of the tile. */
enum class TileMarker {
    NONE,
    /** Filled circle. */ DOT,
    /** Hollow circle. */ RING,
    /** Hollow circle with a slash. */ SLASHED_RING,
    /** Two vertical bars. */ PAUSE,
    /** Filled square. */ SQUARE,
    /** Filled triangle. */ TRIANGLE,
    /** A cross. */ CROSS,
    /** A vertical bar over a dot. */ BANG,
}

/** [fillArgb] is null for a tile drawn as an outline only; [markerArgb] is the glyph's colour, chosen to read on the fill. */
data class TileStyle(val fillArgb: Long?, val outline: TileOutline, val marker: TileMarker, val markerArgb: Long = 0xFFFFFFFF)

val HELMET_STYLE: Map<SocketDrawState, TileStyle> = mapOf(
    SocketDrawState.ABSENT to TileStyle(null, TileOutline.DOTTED, TileMarker.NONE),
    SocketDrawState.IDLE to TileStyle(0xFF94A3B8, TileOutline.THIN, TileMarker.NONE, markerArgb = 0xFF0F172A),
    SocketDrawState.REFUSED to TileStyle(0xFF64748B, TileOutline.THIN, TileMarker.SLASHED_RING),
    SocketDrawState.WRITE_FAILED to TileStyle(0xFFD946EF, TileOutline.THIN, TileMarker.CROSS),
    SocketDrawState.COMMANDED to TileStyle(0xFFF59E0B, TileOutline.THICK, TileMarker.RING, markerArgb = 0xFF0F172A),
    SocketDrawState.DRIVEN to TileStyle(0xFFEF4444, TileOutline.THIN, TileMarker.DOT),
    SocketDrawState.DRIVEN_UNGATED to TileStyle(0xFFEF4444, TileOutline.THIN, TileMarker.TRIANGLE),
    SocketDrawState.WITHHELD to TileStyle(0xFF7C5E10, TileOutline.THICK, TileMarker.PAUSE),
    SocketDrawState.CUT_OFF to TileStyle(0xFF7F1D1D, TileOutline.THICK, TileMarker.SQUARE),
    SocketDrawState.FAULT to TileStyle(0xFFD946EF, TileOutline.THICK, TileMarker.BANG),
)
