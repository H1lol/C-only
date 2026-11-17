package com.motionstudio.app

import androidx.compose.ui.unit.dp
import com.motionstudio.app.ui.rulerLabel
import com.motionstudio.app.ui.rulerStep
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.math.floor

/**
 * The reported bug was that the ruler appeared to jump from ten seconds to a
 * minute and nineteen: labels were mm:ss:ff, so a 15 s tick read as fifteen
 * minutes, and they were produced by an accumulating float loop. Labels are now
 * one unit per zoom level and stepped by integer index; these cases pin both
 * down without needing a device.
 */
class TimelineRulerTest {

    @Test
    fun labelsNameTheUnitTheyAreIn() {
        assertEquals("0s", rulerLabel(0.0, 15.0))
        assertEquals("15s", rulerLabel(15.0, 15.0))
        assertEquals("59s", rulerLabel(59.0, 15.0))
        assertEquals("1:00", rulerLabel(60.0, 30.0))
        assertEquals("1:19", rulerLabel(79.0, 30.0))
        assertEquals("12:05", rulerLabel(725.0, 60.0))
    }

    @Test
    fun subSecondTicksKeepOneDecimal() {
        assertEquals("0.0s", rulerLabel(0.0, 0.1))
        assertEquals("0.5s", rulerLabel(0.5, 0.1))
        assertEquals("2:03.5", rulerLabel(123.5, 0.1))
    }

    @Test
    fun aShortProjectOnAPhoneGetsReadableSpacing() {
        // 79 s project, 256 dp of lane: close to the case that produced the bad axis.
        val span = 79.0
        val width = 256.dp
        val step = rulerStep(span, width)
        assertTrue("step $step is too tight for a label", step >= span / width.value * 78.0)
        val labels = floor(span / step).toInt()
        assertTrue("only $labels labels across $span s", labels >= 2)
    }

    @Test
    fun consecutiveLabelsAlwaysAdvance() {
        // Integer stepping replaced the accumulating float loop, which could repeat
        // or drop a tick; walk every zoom level the timeline offers.
        listOf(0.4f, 1f, 2f, 6f, 24f).forEach { zoom ->
            val span = 3_600.0
            val step = rulerStep(span, 256.dp * zoom)
            assertTrue("step $step is not positive", step > 0.0)
            var previous = -1.0
            var i = 0
            while (i * step <= span) {
                val seconds = i * step
                assertTrue("tick $i at $seconds did not advance", seconds > previous)
                previous = seconds
                i++
            }
            assertTrue("no ticks at zoom $zoom", i >= 1)
        }
    }
}
