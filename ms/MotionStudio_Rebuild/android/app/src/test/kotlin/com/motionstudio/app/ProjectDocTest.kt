package com.motionstudio.app

import com.motionstudio.app.editor.EditorEngine
import com.motionstudio.app.model.ProjectDoc
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Exercises the exact JSON the C++ engine emits (`ProjectSession::to_json`), so
 * a native schema change fails here instead of rendering an empty timeline.
 */
class ProjectDocTest {

    private val nativeJson = """
        {
          "format": "msproj",
          "version": 1,
          "name": "Demo Reel",
          "width": 1080,
          "height": 1920,
          "fps": 60.0,
          "duration": 15.0,
          "media": [
            {"id": 1, "name": "clip.mp4", "path": "/data/clip.mp4", "width": 1920, "height": 1080, "duration": 4.5, "fps": 30.0, "has_audio": true, "type": 0}
          ],
          "tracks": [
            {"id": 1, "name": "V1", "type": 0, "muted": false, "locked": false, "visible": true, "gain": 1.0, "solo": false, "clips": [
              {"id": 7, "media_id": 1, "name": "Clip A", "start": 1.0, "duration": 3.0, "source_in": 0.0, "source_out": 3.0, "speed": 1.0, "opacity": 1.0, "gain": 1.0, "pan": 0.0, "mute": false}
            ]}
          ]
        }
    """.trimIndent()

    @Test
    fun parsesNativeProjectJson() {
        val doc = ProjectDoc.parse(nativeJson)
        assertEquals("Demo Reel", doc.name)
        assertEquals(1080, doc.width)
        assertEquals(1920, doc.height)
        assertEquals(60.0, doc.fps, 0.001)
        assertEquals(1, doc.media.size)
        assertEquals("/data/clip.mp4", doc.media.first().path)
        assertEquals(1, doc.tracks.size)
        assertEquals(1, doc.tracks.first().clips.size)
        assertEquals(7, doc.tracks.first().clips.first().id)
        assertEquals(4.0, doc.tracks.first().clips.first().end, 0.001)
    }

    @Test
    fun timelineEndUsesDeclaredDurationWhenLonger() {
        val doc = ProjectDoc.parse(nativeJson)
        assertEquals(15.0, doc.timelineEnd, 0.001)
    }

    @Test
    fun malformedJsonFallsBackToEmptyProject() {
        val doc = ProjectDoc.parse("not json at all")
        assertEquals(0, doc.tracks.size)
        assertEquals(1920, doc.width)
        assertTrue(doc.timelineEnd >= 1.0)
    }

    @Test
    fun aspectRatioFollowsResolution() {
        assertEquals(0.5625f, ProjectDoc.parse(nativeJson).aspectRatio, 0.0001f)
    }

    @Test
    fun timecodeUsesProjectFrameRate() {
        assertEquals("00:00:00", EditorEngine.formatTimecode(0.0, 30.0))
        assertEquals("00:01:00", EditorEngine.formatTimecode(1.0, 30.0))
        assertEquals("01:02:15", EditorEngine.formatTimecode(62.5, 30.0))
    }

    @Test
    fun clipLookupFindsSelectedClip() {
        val doc = ProjectDoc.parse(nativeJson)
        assertEquals("Clip A", doc.clipById(1, 7)?.name)
        assertEquals(null, doc.clipById(1, 99))
    }
}
