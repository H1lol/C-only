package com.motionstudio.app.model

/**
 * One shape mask on a clip, as the native document reports it.
 *
 * [op] 0 unions this mask with the clip's other masks, 1 subtracts it from that
 * union. Shape: 0 rectangle, 1 ellipse, 2 full frame. x/y/w/h are normalized
 * 0..1 in layer space, feather is in output pixels.
 */
data class ClipMask(
    val shape: Int,
    val x: Float,
    val y: Float,
    val w: Float,
    val h: Float,
    val feather: Float,
    val inverted: Boolean,
    val op: Int,
)
