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
            strokeLineWidth = 2f,
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
            moveTo(11f, 5f)
            arcToRelative(6f, 6f, 0f, true, true, 0f, 12f)
            arcToRelative(6f, 6f, 0f, true, true, 0f, -12f)
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
            moveTo(8f, 6f)
            arcToRelative(3f, 3f, 0f, true, true, 0f, 6f)
            arcToRelative(3f, 3f, 0f, true, true, 0f, -6f)
            moveTo(16f, 6f)
            arcToRelative(3f, 3f, 0f, true, true, 0f, 6f)
            arcToRelative(3f, 3f, 0f, true, true, 0f, -6f)
            moveTo(4f, 19f)
            quadTo(4f, 14.5f, 8.5f, 14.5f)
            lineTo(15.5f, 14.5f)
            quadTo(20f, 14.5f, 20f, 19f)
        }
    }

    val Chat: ImageVector by lazy {
        miStrokeIcon("MiChat") {
            moveTo(5f, 6f)
            lineTo(19f, 6f)
            quadTo(21f, 6f, 21f, 9f)
            lineTo(21f, 15f)
            quadTo(21f, 18f, 18f, 18f)
            lineTo(10f, 16f)
            lineTo(6.5f, 21f)
            lineTo(6.5f, 18f)
            lineTo(6f, 18f)
            quadTo(3f, 18f, 3f, 15f)
            lineTo(3f, 9f)
            quadTo(3f, 6f, 6f, 6f)
        }
    }

    val Settings: ImageVector by lazy {
        miStrokeIcon("MiSettings") {
            moveTo(12f, 9f)
            arcToRelative(3f, 3f, 0f, true, true, 0f, 6f)
            arcToRelative(3f, 3f, 0f, true, true, 0f, -6f)
            moveTo(19.4f, 15f)
            quadTo(19f, 16f, 19.8f, 16.8f)
            quadTo(21f, 18f, 19.8f, 19.2f)
            quadTo(18.6f, 20.4f, 17.4f, 19.2f)
            quadTo(16.6f, 18.4f, 15.6f, 18.8f)
            quadTo(14f, 19.4f, 14f, 20.9f)
            quadTo(14f, 22f, 12f, 22f)
            quadTo(10f, 22f, 10f, 20.9f)
            quadTo(10f, 19.4f, 8.4f, 18.8f)
            quadTo(7.4f, 18.4f, 6.6f, 19.2f)
            quadTo(5.4f, 20.4f, 4.2f, 19.2f)
            quadTo(3f, 18f, 4.2f, 16.8f)
            quadTo(5f, 16f, 4.6f, 15f)
            quadTo(4f, 13.4f, 2.5f, 13.4f)
            quadTo(1f, 13.4f, 1f, 12f)
            quadTo(1f, 10.6f, 2.5f, 10.6f)
            quadTo(4f, 10.6f, 4.6f, 9f)
            quadTo(5f, 8f, 4.2f, 7.2f)
            quadTo(3f, 6f, 4.2f, 4.8f)
            quadTo(5.4f, 3.6f, 6.6f, 4.8f)
            quadTo(7.4f, 5.6f, 8.4f, 5.2f)
            quadTo(10f, 4.6f, 10f, 3.1f)
            quadTo(10f, 2f, 12f, 2f)
            quadTo(14f, 2f, 14f, 3.1f)
            quadTo(14f, 4.6f, 15.6f, 5.2f)
            quadTo(16.6f, 5.6f, 17.4f, 4.8f)
            quadTo(18.6f, 3.6f, 19.8f, 4.8f)
            quadTo(21f, 6f, 19.8f, 7.2f)
            quadTo(19f, 8f, 19.4f, 9f)
            quadTo(20f, 10.6f, 21.5f, 10.6f)
            quadTo(23f, 10.6f, 23f, 12f)
            quadTo(23f, 13.4f, 21.5f, 13.4f)
            quadTo(20f, 13.4f, 19.4f, 15f)
        }
    }

    val Bell: ImageVector by lazy {
        miStrokeIcon("MiBell") {
            moveTo(6f, 16f)
            quadTo(7.3f, 14.5f, 7.8f, 12.8f)
            lineTo(7.8f, 9f)
            quadTo(7.8f, 4.8f, 12f, 4.8f)
            quadTo(16.2f, 4.8f, 16.2f, 9f)
            lineTo(16.2f, 12.8f)
            quadTo(16.7f, 14.5f, 18f, 16f)
            lineTo(18f, 16f)
            moveTo(10f, 18f)
            quadTo(12f, 20f, 14f, 18f)
        }
    }

    val BellOff: ImageVector by lazy {
        miStrokeIcon("MiBellOff") {
            moveTo(6f, 16f)
            quadTo(7.3f, 14.5f, 7.8f, 12.8f)
            lineTo(7.8f, 9f)
            quadTo(7.8f, 7.2f, 8.8f, 6.1f)
            moveTo(16.2f, 11.2f)
            lineTo(16.2f, 12.8f)
            quadTo(16.7f, 14.5f, 18f, 16f)
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
            moveTo(3.8f, 12.8f)
            lineTo(7.2f, 16.2f)
            lineTo(11.4f, 12f)
            moveTo(9.2f, 12.8f)
            lineTo(12.8f, 16.4f)
            lineTo(20.2f, 9f)
        }
    }

    val Delete: ImageVector by lazy {
        miStrokeIcon("MiDelete") {
            moveTo(7f, 6f)
            lineTo(17f, 6f)
            moveTo(10f, 6f)
            lineTo(10.8f, 4.5f)
            lineTo(13.2f, 4.5f)
            lineTo(14f, 6f)
            moveTo(8f, 6f)
            lineTo(8.8f, 19f)
            lineTo(15.2f, 19f)
            lineTo(16f, 6f)
            moveTo(11f, 10f)
            lineTo(11f, 16f)
            moveTo(13f, 10f)
            lineTo(13f, 16f)
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
            moveTo(19f, 12f)
            lineTo(7f, 12f)
            moveTo(11f, 8f)
            lineTo(7f, 12f)
            lineTo(11f, 16f)
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
            moveTo(6f, 5f)
            lineTo(18f, 5f)
            quadTo(20f, 5f, 20f, 7f)
            lineTo(20f, 15f)
            quadTo(20f, 17f, 18f, 17f)
            lineTo(6f, 17f)
            quadTo(4f, 17f, 4f, 15f)
            lineTo(4f, 7f)
            quadTo(4f, 5f, 6f, 5f)
            moveTo(12f, 17f)
            lineTo(12f, 20f)
            moveTo(9f, 20f)
            lineTo(15f, 20f)
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
            moveTo(12f, 3f)
            arcToRelative(9f, 9f, 0f, true, true, 0f, 18f)
            arcToRelative(9f, 9f, 0f, true, true, 0f, -18f)
            moveTo(12f, 7f)
            lineTo(12f, 13f)
            lineTo(16f, 15f)
        }
    }

    val Link: ImageVector by lazy {
        miStrokeIcon("MiLink") {
            moveTo(10f, 14f)
            lineTo(8.4f, 15.6f)
            quadTo(6.2f, 17.8f, 4.6f, 16.2f)
            quadTo(3f, 14.6f, 5.2f, 12.4f)
            lineTo(7.8f, 9.8f)
            quadTo(9.7f, 7.9f, 11.5f, 9.1f)
            moveTo(14f, 10f)
            lineTo(15.6f, 8.4f)
            quadTo(17.8f, 6.2f, 19.4f, 7.8f)
            quadTo(21f, 9.4f, 18.8f, 11.6f)
            lineTo(16.2f, 14.2f)
            quadTo(14.3f, 16.1f, 12.5f, 14.9f)
            moveTo(9f, 15f)
            lineTo(15f, 9f)
        }
    }

    val Call: ImageVector by lazy {
        miStrokeIcon("MiCall") {
            moveTo(19.2f, 17.2f)
            lineTo(19.2f, 19.4f)
            quadTo(19.2f, 20.8f, 17.8f, 20.8f)
            quadTo(12.2f, 20.5f, 8.6f, 16.8f)
            quadTo(4.9f, 13.2f, 4.6f, 7.6f)
            quadTo(4.6f, 6.2f, 6f, 6.2f)
            lineTo(8.2f, 6.2f)
            quadTo(9.4f, 6.2f, 9.8f, 7.3f)
            quadTo(10.1f, 8.6f, 10.6f, 9.8f)
            quadTo(10.9f, 10.5f, 10.3f, 11.1f)
            lineTo(9.2f, 12.2f)
            quadTo(10.8f, 15.2f, 13.8f, 16.8f)
            lineTo(14.9f, 15.7f)
            quadTo(15.5f, 15.1f, 16.2f, 15.4f)
            quadTo(17.4f, 15.9f, 18.7f, 16.2f)
            quadTo(19.2f, 16.4f, 19.2f, 17.2f)
        }
    }

    val Video: ImageVector by lazy {
        miStrokeIcon("MiVideo") {
            moveTo(6f, 7f)
            lineTo(14f, 7f)
            quadTo(16f, 7f, 16f, 9f)
            lineTo(16f, 15f)
            quadTo(16f, 17f, 14f, 17f)
            lineTo(6f, 17f)
            quadTo(4f, 17f, 4f, 15f)
            lineTo(4f, 9f)
            quadTo(4f, 7f, 6f, 7f)
            moveTo(16f, 10f)
            lineTo(20f, 8f)
            lineTo(20f, 16f)
            lineTo(16f, 14f)
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
            moveTo(12f, 3f)
            arcToRelative(9f, 9f, 0f, true, true, 0f, 18f)
            arcToRelative(9f, 9f, 0f, true, true, 0f, -18f)
            moveTo(12f, 8f)
            lineTo(12f, 13.5f)
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
            moveTo(7f, 3f)
            lineTo(14f, 3f)
            lineTo(18f, 7f)
            lineTo(18f, 21f)
            quadTo(18f, 22f, 17f, 22f)
            lineTo(7f, 22f)
            quadTo(6f, 22f, 6f, 21f)
            lineTo(6f, 4f)
            quadTo(6f, 3f, 7f, 3f)
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
            moveTo(6f, 5f)
            lineTo(18f, 5f)
            quadTo(20f, 5f, 20f, 7f)
            lineTo(20f, 17f)
            quadTo(20f, 19f, 18f, 19f)
            lineTo(6f, 19f)
            quadTo(4f, 19f, 4f, 17f)
            lineTo(4f, 7f)
            quadTo(4f, 5f, 6f, 5f)
            moveTo(8f, 11f)
            lineTo(10.5f, 13.5f)
            lineTo(14f, 10f)
            lineTo(20f, 16f)
            moveTo(9f, 9f)
            lineTo(9.1f, 9f)
        }
    }

    val Location: ImageVector by lazy {
        miStrokeIcon("MiLocation") {
            moveTo(12f, 3f)
            quadTo(6f, 3f, 6f, 9f)
            quadTo(6f, 13.8f, 12f, 20f)
            quadTo(18f, 13.8f, 18f, 9f)
            quadTo(18f, 3f, 12f, 3f)
            moveTo(12f, 6.5f)
            arcToRelative(2.5f, 2.5f, 0f, true, true, 0f, 5f)
            arcToRelative(2.5f, 2.5f, 0f, true, true, 0f, -5f)
        }
    }

    val Emoji: ImageVector by lazy {
        miStrokeIcon("MiEmoji") {
            moveTo(12f, 3f)
            arcToRelative(9f, 9f, 0f, true, true, 0f, 18f)
            arcToRelative(9f, 9f, 0f, true, true, 0f, -18f)
            moveTo(9f, 10.5f)
            lineTo(9.1f, 10.5f)
            moveTo(15f, 10.5f)
            lineTo(15.1f, 10.5f)
            moveTo(8.5f, 14.5f)
            quadTo(12f, 18f, 15.5f, 14.5f)
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
            moveTo(16.5f, 6.5f)
            lineTo(16.5f, 15.5f)
            quadTo(16.5f, 20f, 12f, 20f)
            quadTo(7.5f, 20f, 7.5f, 15.5f)
            lineTo(7.5f, 5.5f)
            quadTo(7.5f, 2.5f, 10.5f, 2.5f)
            quadTo(13.5f, 2.5f, 13.5f, 5.5f)
            lineTo(13.5f, 14.5f)
            quadTo(13.5f, 16f, 12f, 16f)
            quadTo(10.5f, 16f, 10.5f, 14.5f)
            lineTo(10.5f, 6.5f)
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
            moveTo(12f, 4f)
            quadTo(9f, 4f, 9f, 7f)
            lineTo(9f, 11f)
            quadTo(9f, 14f, 12f, 14f)
            quadTo(15f, 14f, 15f, 11f)
            lineTo(15f, 7f)
            quadTo(15f, 4f, 12f, 4f)
            moveTo(5f, 11f)
            quadTo(5f, 18f, 12f, 18f)
            quadTo(19f, 18f, 19f, 11f)
            moveTo(12f, 16f)
            lineTo(12f, 21f)
            moveTo(9f, 21f)
            lineTo(15f, 21f)
        }
    }
}
