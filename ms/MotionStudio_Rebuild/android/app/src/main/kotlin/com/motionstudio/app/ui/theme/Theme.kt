package com.motionstudio.app.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp
import com.motionstudio.app.R

/**
 * Dense, near-black workspace with one warm amber accent — the palette of a
 * compositing tool, not a phone gallery. Every surface is a flat panel with a
 * 1 px edge; nothing is rounded or soft, because the numbers carry the colour.
 */
object MsColors {
    val background = Color(0xFF0A0A0C)
    val surface = Color(0xFF101014)
    val surfaceHigh = Color(0xFF16161B)
    val surfaceHighest = Color(0xFF1D1D23)
    val border = Color(0xFF2B2B33)
    val borderSoft = Color(0xFF1F1F26)

    val accent = Color(0xFFFF7A1A)
    val accentSoft = Color(0xFFFFA45C)
    val accentDim = Color(0xFF7A3B0D)

    val textPrimary = Color(0xFFE9E9EC)
    val textSecondary = Color(0xFFB2B2BA)
    val textTertiary = Color(0xFF84848E)
    val textMuted = Color(0xFF57575F)

    val success = Color(0xFF5FD08C)
    val warning = Color(0xFFF0B429)
    val error = Color(0xFFFF5C5C)
    val info = Color(0xFF4CC2D6)

    val viewerBg = Color(0xFF07070A)
    val videoTrack = Color(0xFFB07434)
    val audioTrack = Color(0xFF3E8E78)
    val overlayTrack = Color(0xFF8A6BB8)

    // Node graph + viewport overlays.
    val nodeBg = Color(0xFF141419)
    val nodeBorder = Color(0xFF31313A)
    val wire = Color(0xFF4A4A55)
    val guide = Color(0xFF3C3C46)
    val guideStrong = Color(0xFF5C5C68)
}

/**
 * Inter (SIL OFL 1.1) for interface text and JetBrains Mono (SIL OFL 1.1) for
 * anything numeric: timecode, gain, frame stats, track labels.
 */
object MsFonts {
    val sans = FontFamily(
        Font(R.font.inter_regular, FontWeight.Normal),
        Font(R.font.inter_medium, FontWeight.Medium),
        Font(R.font.inter_semibold, FontWeight.SemiBold),
        Font(R.font.inter_bold, FontWeight.Bold),
    )
    val mono = FontFamily(
        Font(R.font.jbmono_regular, FontWeight.Normal),
        Font(R.font.jbmono_medium, FontWeight.Medium),
        Font(R.font.jbmono_bold, FontWeight.Bold),
    )
}

private val MsTypography = Typography(
    displaySmall = TextStyle(fontFamily = MsFonts.sans, fontSize = 34.sp, lineHeight = 40.sp, fontWeight = FontWeight.Bold),
    headlineMedium = TextStyle(fontFamily = MsFonts.sans, fontSize = 24.sp, lineHeight = 30.sp, fontWeight = FontWeight.Bold),
    titleMedium = TextStyle(fontFamily = MsFonts.sans, fontSize = 16.sp, lineHeight = 22.sp, fontWeight = FontWeight.SemiBold),
    titleSmall = TextStyle(fontFamily = MsFonts.sans, fontSize = 14.sp, lineHeight = 20.sp, fontWeight = FontWeight.SemiBold),
    bodyMedium = TextStyle(fontFamily = MsFonts.sans, fontSize = 14.sp, lineHeight = 20.sp),
    bodySmall = TextStyle(fontFamily = MsFonts.sans, fontSize = 12.sp, lineHeight = 16.sp),
    labelMedium = TextStyle(fontFamily = MsFonts.sans, fontSize = 12.sp, lineHeight = 16.sp, fontWeight = FontWeight.Medium),
    labelSmall = TextStyle(fontFamily = MsFonts.sans, fontSize = 11.sp, lineHeight = 14.sp, fontWeight = FontWeight.Medium),
)

@Composable
fun MotionStudioTheme(accent: Color = MsColors.accent, content: @Composable () -> Unit) {
    val dark = isSystemInDarkTheme()
    val scheme = darkColorScheme(
        primary = accent,
        onPrimary = Color.White,
        primaryContainer = accent.copy(alpha = 0.20f),
        onPrimaryContainer = MsColors.accentSoft,
        secondary = MsColors.accentSoft,
        background = MsColors.background,
        onBackground = MsColors.textPrimary,
        surface = MsColors.surface,
        onSurface = MsColors.textPrimary,
        surfaceVariant = MsColors.surfaceHigh,
        onSurfaceVariant = MsColors.textSecondary,
        outline = MsColors.border,
        error = MsColors.error,
    )
    MaterialTheme(
        colorScheme = if (dark) scheme else scheme,
        typography = MsTypography.copy(
            bodyLarge = MsTypography.bodyMedium,
            labelLarge = MsTypography.labelMedium,
        ),
        content = content,
    )
}
