package mi.e2ee.android.ui

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.PathFillType
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.PathBuilder
import androidx.compose.ui.graphics.vector.path
import androidx.compose.ui.unit.dp

private fun miStrokeIcon(name: String, block: PathBuilder.() -> Unit): ImageVector {
    return ImageVector.Builder(
        name = name,
        defaultWidth = 24.dp,
        defaultHeight = 24.dp,
        viewportWidth = 24f,
        viewportHeight = 24f
    ).apply {
        path(
            fill = SolidColor(Color.Transparent),
            stroke = SolidColor(Color(0xFF000000)),
            strokeLineWidth = 1.9f,
            strokeLineCap = StrokeCap.Round,
            strokeLineJoin = StrokeJoin.Round,
            strokeLineMiter = 4f,
            pathFillType = PathFillType.NonZero
        ) {
            block()
        }
    }.build()
}

object MiOwnedIcons {
    val Add: ImageVector by lazy {
        miStrokeIcon("MiAdd") {
            moveTo(12f, 5f)
            lineTo(12f, 19f)
            moveTo(5f, 12f)
            lineTo(19f, 12f)
        }
    }

    val Search: ImageVector by lazy {
        miStrokeIcon("MiSearch") {
            moveTo(11f, 4f)
            arcToRelative(7f, 7f, 0f, true, true, 0f, 14f)
            arcToRelative(7f, 7f, 0f, true, true, 0f, -14f)
            moveTo(16.5f, 16.5f)
            lineTo(20f, 20f)
        }
    }

    val Person: ImageVector by lazy {
        miStrokeIcon("MiPerson") {
            moveTo(12f, 5f)
            arcToRelative(3f, 3f, 0f, true, true, 0f, 6f)
            arcToRelative(3f, 3f, 0f, true, true, 0f, -6f)
            moveTo(5f, 19f)
            quadTo(12f, 13.7f, 19f, 19f)
        }
    }

    val Group: ImageVector by lazy {
        miStrokeIcon("MiGroup") {
            moveTo(9f, 7f)
            arcToRelative(2.2f, 2.2f, 0f, true, true, 0f, 4.4f)
            arcToRelative(2.2f, 2.2f, 0f, true, true, 0f, -4.4f)
            moveTo(15.7f, 7.8f)
            arcToRelative(1.8f, 1.8f, 0f, true, true, 0f, 3.6f)
            arcToRelative(1.8f, 1.8f, 0f, true, true, 0f, -3.6f)
            moveTo(4.8f, 18.4f)
            quadTo(9f, 14.7f, 13.3f, 18.4f)
            moveTo(12.9f, 18.4f)
            quadTo(15.9f, 15.8f, 19.2f, 18.4f)
        }
    }

    val Chat: ImageVector by lazy {
        miStrokeIcon("MiChat") {
            moveTo(5f, 6f)
            lineTo(19f, 6f)
            quadTo(21f, 6f, 21f, 8f)
            lineTo(21f, 14f)
            quadTo(21f, 16f, 19f, 16f)
            lineTo(10f, 16f)
            lineTo(6f, 19f)
            lineTo(7f, 16f)
            lineTo(5f, 16f)
            quadTo(3f, 16f, 3f, 14f)
            lineTo(3f, 8f)
            quadTo(3f, 6f, 5f, 6f)
        }
    }

    val Settings: ImageVector by lazy {
        miStrokeIcon("MiSettings") {
            moveTo(12f, 7f)
            arcToRelative(5f, 5f, 0f, true, true, 0f, 10f)
            arcToRelative(5f, 5f, 0f, true, true, 0f, -10f)
            moveTo(12f, 4f)
            lineTo(12f, 5.5f)
            moveTo(12f, 18.5f)
            lineTo(12f, 20f)
            moveTo(4f, 12f)
            lineTo(5.5f, 12f)
            moveTo(18.5f, 12f)
            lineTo(20f, 12f)
            moveTo(6.4f, 6.4f)
            lineTo(7.5f, 7.5f)
            moveTo(16.5f, 16.5f)
            lineTo(17.6f, 17.6f)
            moveTo(16.5f, 7.5f)
            lineTo(17.6f, 6.4f)
            moveTo(6.4f, 17.6f)
            lineTo(7.5f, 16.5f)
        }
    }

    val Bell: ImageVector by lazy {
        miStrokeIcon("MiBell") {
            moveTo(8f, 16f)
            lineTo(8f, 11f)
            quadTo(8f, 6.4f, 12f, 6.4f)
            quadTo(16f, 6.4f, 16f, 11f)
            lineTo(16f, 16f)
            moveTo(6f, 16f)
            lineTo(18f, 16f)
            moveTo(10f, 18f)
            quadTo(12f, 20f, 14f, 18f)
        }
    }

    val BellOff: ImageVector by lazy {
        miStrokeIcon("MiBellOff") {
            moveTo(8f, 16f)
            lineTo(8f, 12.2f)
            quadTo(8f, 8f, 11.5f, 7.3f)
            moveTo(16f, 11f)
            lineTo(16f, 16f)
            moveTo(6f, 16f)
            lineTo(18f, 16f)
            moveTo(10f, 18f)
            quadTo(12f, 20f, 14f, 18f)
            moveTo(5f, 5f)
            lineTo(19f, 19f)
        }
    }

    val Pin: ImageVector by lazy {
        miStrokeIcon("MiPin") {
            moveTo(8f, 7f)
            lineTo(16f, 7f)
            lineTo(14f, 11f)
            lineTo(14f, 14f)
            lineTo(10f, 14f)
            lineTo(10f, 11f)
            close()
            moveTo(12f, 14f)
            lineTo(12f, 20f)
        }
    }

    val Check: ImageVector by lazy {
        miStrokeIcon("MiCheck") {
            moveTo(6f, 12.5f)
            lineTo(10f, 16.5f)
            lineTo(18f, 8.5f)
        }
    }

    val CheckDouble: ImageVector by lazy {
        miStrokeIcon("MiCheckDouble") {
            moveTo(4.5f, 12.5f)
            lineTo(7.5f, 15.5f)
            lineTo(11.5f, 11.5f)
            moveTo(10f, 12.5f)
            lineTo(13f, 15.5f)
            lineTo(19.5f, 9f)
        }
    }

    val Delete: ImageVector by lazy {
        miStrokeIcon("MiDelete") {
            moveTo(8f, 7f)
            lineTo(16f, 7f)
            moveTo(10f, 7f)
            lineTo(10.8f, 5f)
            lineTo(13.2f, 5f)
            lineTo(14f, 7f)
            moveTo(9f, 7f)
            lineTo(9.8f, 18f)
            lineTo(14.2f, 18f)
            lineTo(15f, 7f)
            moveTo(11f, 10f)
            lineTo(11f, 15f)
            moveTo(13f, 10f)
            lineTo(13f, 15f)
        }
    }

    val ChevronRight: ImageVector by lazy {
        miStrokeIcon("MiChevronRight") {
            moveTo(9f, 6f)
            lineTo(15f, 12f)
            lineTo(9f, 18f)
        }
    }

    val ChevronDown: ImageVector by lazy {
        miStrokeIcon("MiChevronDown") {
            moveTo(6f, 9f)
            lineTo(12f, 15f)
            lineTo(18f, 9f)
        }
    }

    val ArrowBack: ImageVector by lazy {
        miStrokeIcon("MiArrowBack") {
            moveTo(14.8f, 6f)
            lineTo(8.8f, 12f)
            lineTo(14.8f, 18f)
            moveTo(9f, 12f)
            lineTo(19f, 12f)
        }
    }

    val Shield: ImageVector by lazy {
        miStrokeIcon("MiShield") {
            moveTo(12f, 4f)
            lineTo(18f, 7f)
            lineTo(18f, 12f)
            quadTo(18f, 17f, 12f, 20f)
            quadTo(6f, 17f, 6f, 12f)
            lineTo(6f, 7f)
            close()
        }
    }

    val ShieldCheck: ImageVector by lazy {
        miStrokeIcon("MiShieldCheck") {
            moveTo(12f, 4f)
            lineTo(18f, 7f)
            lineTo(18f, 12f)
            quadTo(18f, 17f, 12f, 20f)
            quadTo(6f, 17f, 6f, 12f)
            lineTo(6f, 7f)
            close()
            moveTo(9.2f, 12.2f)
            lineTo(11.2f, 14.2f)
            lineTo(14.9f, 10.4f)
        }
    }

    val Eye: ImageVector by lazy {
        miStrokeIcon("MiEye") {
            moveTo(2.5f, 12f)
            quadTo(6.5f, 6f, 12f, 6f)
            quadTo(17.5f, 6f, 21.5f, 12f)
            quadTo(17.5f, 18f, 12f, 18f)
            quadTo(6.5f, 18f, 2.5f, 12f)
            moveTo(12f, 9.5f)
            arcToRelative(2.5f, 2.5f, 0f, true, true, 0f, 5f)
            arcToRelative(2.5f, 2.5f, 0f, true, true, 0f, -5f)
        }
    }

    val Devices: ImageVector by lazy {
        miStrokeIcon("MiDevices") {
            moveTo(4f, 7f)
            quadTo(4f, 6f, 5f, 6f)
            lineTo(10f, 6f)
            quadTo(11f, 6f, 11f, 7f)
            lineTo(11f, 17f)
            quadTo(11f, 18f, 10f, 18f)
            lineTo(5f, 18f)
            quadTo(4f, 18f, 4f, 17f)
            close()
            moveTo(7.5f, 16.1f)
            lineTo(7.6f, 16.1f)
            moveTo(13f, 8f)
            quadTo(13f, 7f, 14f, 7f)
            lineTo(19f, 7f)
            quadTo(20f, 7f, 20f, 8f)
            lineTo(20f, 15f)
            quadTo(20f, 16f, 19f, 16f)
            lineTo(14f, 16f)
            quadTo(13f, 16f, 13f, 15f)
            close()
        }
    }

    val Bug: ImageVector by lazy {
        miStrokeIcon("MiBug") {
            moveTo(12f, 8f)
            arcToRelative(3f, 3f, 0f, true, true, 0f, 6f)
            arcToRelative(3f, 3f, 0f, true, true, 0f, -6f)
            moveTo(12f, 14f)
            lineTo(12f, 19f)
            moveTo(9f, 7f)
            lineTo(7.2f, 5.2f)
            moveTo(15f, 7f)
            lineTo(16.8f, 5.2f)
            moveTo(7f, 11f)
            lineTo(4.5f, 11f)
            moveTo(19.5f, 11f)
            lineTo(17f, 11f)
            moveTo(7.5f, 15f)
            lineTo(5.5f, 17f)
            moveTo(16.5f, 15f)
            lineTo(18.5f, 17f)
        }
    }

    val Clock: ImageVector by lazy {
        miStrokeIcon("MiClock") {
            moveTo(12f, 4f)
            arcToRelative(8f, 8f, 0f, true, true, 0f, 16f)
            arcToRelative(8f, 8f, 0f, true, true, 0f, -16f)
            moveTo(12f, 8f)
            lineTo(12f, 12f)
            lineTo(15f, 13.8f)
        }
    }

    val Link: ImageVector by lazy {
        miStrokeIcon("MiLink") {
            moveTo(8.3f, 14.8f)
            lineTo(6.4f, 16.7f)
            quadTo(4.2f, 18.9f, 2.8f, 17.5f)
            quadTo(1.5f, 16.2f, 3.7f, 13.9f)
            lineTo(6.3f, 11.3f)
            quadTo(8.2f, 9.4f, 10f, 10.6f)
            moveTo(15.7f, 9.2f)
            lineTo(17.6f, 7.3f)
            quadTo(19.8f, 5.1f, 21.2f, 6.5f)
            quadTo(22.5f, 7.8f, 20.3f, 10.1f)
            lineTo(17.7f, 12.7f)
            quadTo(15.8f, 14.6f, 14f, 13.4f)
            moveTo(9f, 15f)
            lineTo(15f, 9f)
        }
    }

    val Call: ImageVector by lazy {
        miStrokeIcon("MiCall") {
            moveTo(7f, 5.7f)
            quadTo(5f, 8.4f, 7.6f, 13f)
            quadTo(10.2f, 17.6f, 14.8f, 19.7f)
            quadTo(17.7f, 21f, 18.9f, 18.2f)
            lineTo(19.6f, 16.5f)
            lineTo(15.8f, 14.9f)
            lineTo(14.5f, 16.7f)
            quadTo(11.5f, 15.3f, 9.9f, 12.1f)
            lineTo(11.7f, 10.8f)
            lineTo(10.1f, 7f)
            close()
        }
    }

    val Video: ImageVector by lazy {
        miStrokeIcon("MiVideo") {
            moveTo(4f, 8f)
            quadTo(4f, 7f, 5f, 7f)
            lineTo(13f, 7f)
            quadTo(14f, 7f, 14f, 8f)
            lineTo(14f, 16f)
            quadTo(14f, 17f, 13f, 17f)
            lineTo(5f, 17f)
            quadTo(4f, 17f, 4f, 16f)
            close()
            moveTo(14f, 11f)
            lineTo(20f, 8.5f)
            lineTo(20f, 15.5f)
            close()
        }
    }

    val Lock: ImageVector by lazy {
        miStrokeIcon("MiLock") {
            moveTo(8f, 11f)
            lineTo(8f, 9f)
            quadTo(8f, 6f, 12f, 6f)
            quadTo(16f, 6f, 16f, 9f)
            lineTo(16f, 11f)
            moveTo(7f, 11f)
            lineTo(17f, 11f)
            lineTo(17f, 19f)
            lineTo(7f, 19f)
            close()
            moveTo(12f, 14f)
            lineTo(12f, 16f)
        }
    }

    val Alert: ImageVector by lazy {
        miStrokeIcon("MiAlert") {
            moveTo(12f, 4f)
            arcToRelative(8f, 8f, 0f, true, true, 0f, 16f)
            arcToRelative(8f, 8f, 0f, true, true, 0f, -16f)
            moveTo(12f, 8f)
            lineTo(12f, 13f)
            moveTo(12f, 16.5f)
            lineTo(12.1f, 16.5f)
        }
    }

    val Close: ImageVector by lazy {
        miStrokeIcon("MiClose") {
            moveTo(6.5f, 6.5f)
            lineTo(17.5f, 17.5f)
            moveTo(17.5f, 6.5f)
            lineTo(6.5f, 17.5f)
        }
    }

    val Reply: ImageVector by lazy {
        miStrokeIcon("MiReply") {
            moveTo(9f, 8f)
            lineTo(5f, 12f)
            lineTo(9f, 16f)
            moveTo(5.5f, 12f)
            lineTo(13.5f, 12f)
            quadTo(18.5f, 12f, 19.2f, 17f)
        }
    }

    val Forward: ImageVector by lazy {
        miStrokeIcon("MiForward") {
            moveTo(15f, 8f)
            lineTo(19f, 12f)
            lineTo(15f, 16f)
            moveTo(18.5f, 12f)
            lineTo(10.5f, 12f)
            quadTo(5.5f, 12f, 4.8f, 17f)
        }
    }

    val Undo: ImageVector by lazy {
        miStrokeIcon("MiUndo") {
            moveTo(10f, 8f)
            lineTo(6f, 12f)
            lineTo(10f, 16f)
            moveTo(6.5f, 12f)
            lineTo(14f, 12f)
            quadTo(19f, 12f, 19f, 17f)
        }
    }

    val File: ImageVector by lazy {
        miStrokeIcon("MiFile") {
            moveTo(7f, 4f)
            lineTo(14f, 4f)
            lineTo(18f, 8f)
            lineTo(18f, 20f)
            lineTo(7f, 20f)
            close()
            moveTo(14f, 4f)
            lineTo(14f, 8f)
            lineTo(18f, 8f)
        }
    }

    val Copy: ImageVector by lazy {
        miStrokeIcon("MiCopy") {
            moveTo(8f, 8f)
            lineTo(8f, 18f)
            lineTo(16f, 18f)
            lineTo(16f, 8f)
            close()
            moveTo(11f, 5f)
            lineTo(19f, 5f)
            lineTo(19f, 15f)
        }
    }

    val Play: ImageVector by lazy {
        miStrokeIcon("MiPlay") {
            moveTo(8.5f, 7f)
            lineTo(16.5f, 12f)
            lineTo(8.5f, 17f)
            close()
        }
    }

    val Photo: ImageVector by lazy {
        miStrokeIcon("MiPhoto") {
            moveTo(4f, 7f)
            lineTo(20f, 7f)
            lineTo(20f, 17f)
            lineTo(4f, 17f)
            close()
            moveTo(7.5f, 10f)
            lineTo(7.6f, 10f)
            moveTo(6.5f, 15f)
            lineTo(10.5f, 11f)
            lineTo(13f, 13.5f)
            lineTo(15f, 11.5f)
            lineTo(17.5f, 15f)
        }
    }

    val Location: ImageVector by lazy {
        miStrokeIcon("MiLocation") {
            moveTo(12f, 5.5f)
            arcToRelative(4f, 4f, 0f, true, true, 0f, 8f)
            arcToRelative(4f, 4f, 0f, true, true, 0f, -8f)
            moveTo(12f, 13.5f)
            lineTo(12f, 20f)
        }
    }

    val Emoji: ImageVector by lazy {
        miStrokeIcon("MiEmoji") {
            moveTo(12f, 4f)
            arcToRelative(8f, 8f, 0f, true, true, 0f, 16f)
            arcToRelative(8f, 8f, 0f, true, true, 0f, -16f)
            moveTo(9f, 10f)
            lineTo(9.1f, 10f)
            moveTo(15f, 10f)
            lineTo(15.1f, 10f)
            moveTo(8.7f, 14f)
            quadTo(12f, 16.7f, 15.3f, 14f)
        }
    }

    val Star: ImageVector by lazy {
        miStrokeIcon("MiStar") {
            moveTo(12f, 5f)
            lineTo(14f, 9.3f)
            lineTo(18.7f, 10f)
            lineTo(15.3f, 13.3f)
            lineTo(16.1f, 18f)
            lineTo(12f, 15.8f)
            lineTo(7.9f, 18f)
            lineTo(8.7f, 13.3f)
            lineTo(5.3f, 10f)
            lineTo(10f, 9.3f)
            close()
        }
    }

    val PersonAdd: ImageVector by lazy {
        miStrokeIcon("MiPersonAdd") {
            moveTo(9f, 7f)
            arcToRelative(2.6f, 2.6f, 0f, true, true, 0f, 5.2f)
            arcToRelative(2.6f, 2.6f, 0f, true, true, 0f, -5.2f)
            moveTo(4.8f, 18f)
            quadTo(9f, 14.4f, 13.2f, 18f)
            moveTo(17f, 9f)
            lineTo(17f, 14f)
            moveTo(14.5f, 11.5f)
            lineTo(19.5f, 11.5f)
        }
    }

    val Attach: ImageVector by lazy {
        miStrokeIcon("MiAttach") {
            moveTo(9f, 12.5f)
            lineTo(13.8f, 7.7f)
            quadTo(15.6f, 5.9f, 17.3f, 7.6f)
            quadTo(19f, 9.3f, 17.2f, 11.1f)
            lineTo(10.8f, 17.5f)
            quadTo(8.1f, 20.2f, 5.8f, 17.9f)
            quadTo(3.6f, 15.7f, 6.3f, 13f)
            lineTo(12.1f, 7.2f)
        }
    }

    val Send: ImageVector by lazy {
        miStrokeIcon("MiSend") {
            moveTo(4f, 12f)
            lineTo(20f, 5f)
            lineTo(14f, 19f)
            lineTo(11.5f, 13f)
            close()
            moveTo(11.5f, 13f)
            lineTo(8f, 12f)
        }
    }

    val Mic: ImageVector by lazy {
        miStrokeIcon("MiMic") {
            moveTo(12f, 6f)
            quadTo(10f, 6f, 10f, 8f)
            lineTo(10f, 12f)
            quadTo(10f, 14f, 12f, 14f)
            quadTo(14f, 14f, 14f, 12f)
            lineTo(14f, 8f)
            quadTo(14f, 6f, 12f, 6f)
            close()
            moveTo(8f, 11.5f)
            quadTo(8f, 16f, 12f, 16f)
            quadTo(16f, 16f, 16f, 11.5f)
            moveTo(12f, 16f)
            lineTo(12f, 19f)
            moveTo(9.5f, 19f)
            lineTo(14.5f, 19f)
        }
    }
}
