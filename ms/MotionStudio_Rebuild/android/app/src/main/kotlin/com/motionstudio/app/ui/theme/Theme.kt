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

/** Quiet charcoal surfaces with a single violet accent. */
object MsColors {
    val background = Color(0xFF101014)
    val surface = Color(0xFF18181E)
    val surfaceHigh = Color(0xFF202027)
    val surfaceHighest = Color(0xFF2A2A33)
    val border = Color(0xFF33333D)

    val accent = Color(0xFF9476F5)
    val accentSoft = Color(0xFFB6A2FF)
    val accentDim = Color(0xFF59459E)

    val textPrimary = Color(0xFFF0EFF5)
    val textSecondary = Color(0xFFC4C1CE)
    val textTertiary = Color(0xFF918D9D)
    val textMuted = Color(0xFF656170)

    val success = Color(0xFF54C99A)
    val warning = Color(0xFFE8B45F)
    val error = Color(0xFFE87979)

    val viewerBg = Color(0xFF0B0B0E)
    val videoTrack = Color(0xFF8B79E8)
    val audioTrack = Color(0xFF58B997)
    val overlayTrack = Color(0xFFB17BD9)
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
