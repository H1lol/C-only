package com.motionstudio.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.motionstudio.app.editor.EditorEngine
import com.motionstudio.app.ui.theme.MsColors
import com.motionstudio.app.ui.theme.MsFonts

/**
 * The clip's render chain as a left-to-right graph: source, every effect in
 * stack order, then transform, grade, look and output. The chain is real — each
 * card is a stage the C++ renderer actually executes for this clip, in this
 * order. Tapping a card selects it and its parameters appear underneath.
 */
private data class ChainNode(
    val key: String,
    val kind: String,
    val title: String,
    val detail: String,
    val effectIndex: Int = -1,
    val enabled: Boolean = true,
)

@Composable
fun NodeGraphPane(engine: EditorEngine, modifier: Modifier = Modifier) {
    val clip = engine.doc.clipById(engine.selectedTrackId, engine.selectedClipId)
    val effects = clip?.effects ?: emptyList()

    val chain = remember(clip?.id, effects, engine.lookName, engine.gradeActive, clip?.isText) {
        buildList {
            add(
                ChainNode(
                    "src", "SRC",
                    clip?.name ?: "no clip",
                    clip?.let { "%.2fs".format(it.duration) } ?: "select a clip",
                )
            )
            effects.forEachIndexed { index, e ->
                add(
                    ChainNode(
                        "fx$index", "FX",
                        effectLabel(e.id),
                        if (e.params.isEmpty()) "defaults" else "${e.params.size} params",
                        effectIndex = index,
                        enabled = e.enabled,
                    )
                )
            }
            add(ChainNode("xform", "XFM", "Transform", "scale · pos · opacity"))
            add(
                ChainNode(
                    "grade", "GRD", "Grade",
                    if (engine.gradeActive) "active" else "bypassed",
                    enabled = engine.gradeActive,
                )
            )
            add(
                ChainNode(
                    "look", "LUT", "Look",
                    engine.lookName ?: "none",
                    enabled = engine.lookName != null,
                )
            )
            add(ChainNode("out", "OUT", "Output", "${engine.doc.width}×${engine.doc.height}"))
        }
    }

    var selectedKey by remember(chain.size) { mutableStateOf(chain.lastOrNull()?.key ?: "") }
    val selected = chain.firstOrNull { it.key == selectedKey } ?: chain.lastOrNull()

    Column(modifier.background(MsColors.background)) {
        Row(
            Modifier.fillMaxWidth().height(28.dp).background(MsColors.surfaceHigh)
                .padding(horizontal = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                "RENDER CHAIN",
                color = MsColors.textMuted,
                fontSize = 9.sp,
                fontFamily = MsFonts.mono,
                letterSpacing = 1.sp,
            )
            Spacer(Modifier.weight(1f))
            Text(
                "${effects.size} fx",
                color = MsColors.textTertiary,
                fontSize = 9.sp,
                fontFamily = MsFonts.mono,
            )
        }

        Row(
            Modifier
                .fillMaxWidth()
                .background(MsColors.viewerBg)
                .horizontalScroll(rememberScrollState())
                .padding(vertical = 12.dp, horizontal = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            chain.forEachIndexed { index, node ->
                NodeCard(
                    node = node,
                    selected = node.key == selected?.key,
                    onClick = { selectedKey = node.key },
                )
                if (index != chain.lastIndex) {
                    // Wire to the next node. Drawn as a rule so the chain reads
                    // as a flow even at phone widths.
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Box(Modifier.width(14.dp).height(1.dp).background(MsColors.wire))
                        Box(
                            Modifier.size(4.dp).background(
                                if (node.enabled) MsColors.wire else MsColors.border,
                                CircleShape,
                            )
                        )
                        Box(Modifier.width(14.dp).height(1.dp).background(MsColors.wire))
                    }
                }
            }
        }

        Column(
            Modifier
                .fillMaxWidth()
                .weight(1f)
                .background(MsColors.surface)
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 12.dp, vertical = 8.dp)
        ) {
            val node = selected
            if (node == null) {
                Text("No clip selected", color = MsColors.textTertiary, fontSize = 12.sp)
                return@Column
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    node.kind,
                    color = MsColors.accent,
                    fontSize = 10.sp,
                    fontFamily = MsFonts.mono,
                    fontWeight = FontWeight.Bold,
                )
                Spacer(Modifier.width(8.dp))
                Text(node.title, color = MsColors.textPrimary, fontSize = 14.sp, fontWeight = FontWeight.Medium)
                Spacer(Modifier.weight(1f))
                Text(
                    if (node.enabled) "ACTIVE" else "BYPASS",
                    color = if (node.enabled) MsColors.success else MsColors.textMuted,
                    fontSize = 9.sp,
                    fontFamily = MsFonts.mono,
                )
            }
            Text(node.detail, color = MsColors.textMuted, fontSize = 11.sp)
            Spacer(Modifier.height(8.dp))

            when {
                node.effectIndex >= 0 && clip != null -> {
                    val effect = clip.effects.getOrNull(node.effectIndex)
                    val def = MsEffects.firstOrNull { it.id == effect?.id }
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        TextButton(onClick = { engine.setEffectEnabled(node.effectIndex, !node.enabled) }) {
                            Text(
                                if (node.enabled) "Bypass" else "Enable",
                                color = MsColors.accentSoft,
                                fontSize = 11.sp,
                            )
                        }
                        TextButton(onClick = { engine.removeEffect(node.effectIndex) }) {
                            Text("Delete node", color = MsColors.error, fontSize = 11.sp)
                        }
                    }
                    if (def == null) {
                        Text(
                            "No parameter map for “${effect?.id}” in the UI",
                            color = MsColors.textMuted,
                            fontSize = 11.sp,
                        )
                    } else {
                        def.params.forEach { param ->
                            val value = (effect?.params?.get(param.name)?.toFloat() ?: param.default)
                                .coerceIn(param.min, param.max)
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Text(
                                    param.label.uppercase(),
                                    color = MsColors.textTertiary,
                                    fontSize = 9.sp,
                                    fontFamily = MsFonts.mono,
                                    modifier = Modifier.width(74.dp),
                                )
                                Slider(
                                    value = value,
                                    onValueChange = {
                                        engine.setEffectParam(node.effectIndex, param.name, it)
                                    },
                                    valueRange = param.min..param.max,
                                    colors = SliderDefaults.colors(
                                        thumbColor = MsColors.accent,
                                        activeTrackColor = MsColors.accent,
                                        inactiveTrackColor = MsColors.border,
                                    ),
                                    modifier = Modifier.weight(1f),
                                )
                                Text(
                                    "%.2f".format(value),
                                    color = MsColors.textSecondary,
                                    fontSize = 10.sp,
                                    fontFamily = MsFonts.mono,
                                    modifier = Modifier.width(44.dp),
                                )
                            }
                        }
                        Text(
                            def.note,
                            color = MsColors.textMuted,
                            fontSize = 10.sp,
                        )
                    }
                }

                node.kind == "GRD" -> {
                    Text(
                        "Seven-parameter grade is applied after the clip's effects. " +
                            "Open the GRADE tab for the full set and the histogram.",
                        color = MsColors.textSecondary,
                        fontSize = 11.sp,
                    )
                }

                node.kind == "XFM" -> {
                    Text(
                        "Scale, position and opacity can be animated from the KEYS tab.",
                        color = MsColors.textSecondary,
                        fontSize = 11.sp,
                    )
                }

                else -> {
                    Text(
                        "Terminal stage — nothing to configure here.",
                        color = MsColors.textMuted,
                        fontSize = 11.sp,
                    )
                }
            }
        }
    }
}

@Composable
private fun NodeCard(node: ChainNode, selected: Boolean, onClick: () -> Unit) {
    Column(
        Modifier
            .width(104.dp)
            .background(MsColors.nodeBg, RoundedCornerShape(3.dp))
            .border(
                1.dp,
                if (selected) MsColors.accent else MsColors.nodeBorder,
                RoundedCornerShape(3.dp),
            )
            .clickable(onClick = onClick)
            .padding(horizontal = 8.dp, vertical = 6.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                node.kind,
                color = if (selected) MsColors.accent else MsColors.textTertiary,
                fontSize = 9.sp,
                fontFamily = MsFonts.mono,
                fontWeight = FontWeight.Bold,
            )
            Spacer(Modifier.weight(1f))
            Box(
                Modifier.size(5.dp).background(
                    if (node.enabled) MsColors.success else MsColors.textMuted,
                    CircleShape,
                )
            )
        }
        Spacer(Modifier.height(3.dp))
        Text(
            node.title,
            color = MsColors.textPrimary,
            fontSize = 11.sp,
            maxLines = 1,
        )
        Text(
            node.detail,
            color = MsColors.textMuted,
            fontSize = 9.sp,
            fontFamily = MsFonts.mono,
            maxLines = 1,
        )
    }
}

/** Human label for an effect id, falling back to the raw id. */
private fun effectLabel(id: String): String =
    MsEffects.firstOrNull { it.id == id }?.name ?: id.replace('_', ' ')
