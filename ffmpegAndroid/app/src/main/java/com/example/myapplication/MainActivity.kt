package com.example.myapplication

import android.content.ContentValues
import android.content.Context
import android.os.Bundle
import android.os.Environment
import android.provider.MediaStore
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.example.myapplication.ui.theme.MyApplicationTheme
import com.example.ffmpeg.Mp4ParserJNI
import java.io.File
import kotlinx.coroutines.launch
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        setContent {
            MyApplicationTheme {
                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    Mp4ParserScreen(modifier = Modifier.padding(innerPadding))
                }
            }
        }
    }
}
@Composable
fun Mp4ParserScreen(modifier: Modifier = Modifier) {
    val context = LocalContext.current
    val parser = remember { Mp4ParserJNI() }

    var logText by remember { mutableStateOf("") }
    var isLoading by remember { mutableStateOf(false) }

    // 拷贝 assets/output.mp4 到 cacheDir/output.mp4（只拷贝一次）
    LaunchedEffect(Unit) {
        copyAssetToCache(context, "output.mp4", "output.mp4")
        logText += "Copied output.mp4 to cacheDir\n"
    }

    val cacheFile = File(context.cacheDir, "output.mp4")
    val mp4Path = cacheFile.absolutePath
    val coroutineScope = rememberCoroutineScope()

    Column(modifier = modifier.padding(16.dp)) {
        Button(
            onClick = {
                logText = "" // Clear log before new operation
                logText += "Opening file...\n"
                coroutineScope.launch {
                    isLoading = true
                    val opened = withContext(Dispatchers.IO) {
                        if (!cacheFile.exists()) {
                            false
                        } else {
                            parser.open(mp4Path)
                        }
                    }
                    logText += "Open file: $opened\n"
                    isLoading = false
                }
            },
            enabled = !isLoading
        ) {
            Text("Open MP4 File")
        }

        Spacer(modifier = Modifier.height(8.dp))

        Button(
            onClick = {
                logText = "" // Clear log before new operation
                logText += "Reading next video packet...\n"
                coroutineScope.launch {
                    isLoading = true
                    val packet = withContext(Dispatchers.IO) { parser.readNextVideoPacket() }
                    if (packet != null) {
                        logText += "Next Packet: PTS=${packet.pts}, DTS=${packet.dts}, Stream=${packet.streamIndex}\n"
                    } else {
                        logText += "No more packets or failed to read packet.\n"
                    }
                    isLoading = false
                }
            },
            enabled = !isLoading
        ) {
            Text("Read Next Video Packet")
        }

        Spacer(modifier = Modifier.height(8.dp))

        Button(
            onClick = {
                logText = "" // Clear log before new operation
                logText += "Reading all video packets...\n"
                coroutineScope.launch {
                    isLoading = true
                    val allPackets = withContext(Dispatchers.IO) { parser.readAllVideoPackets() }
                    if (!allPackets.isNullOrEmpty()) {
                        logText += "Total packets read: ${allPackets.size}\n"
                        allPackets.take(20).forEachIndexed { idx, packet ->
                            logText += "Packet $idx: PTS=${packet.pts}, DTS=${packet.dts}, Stream=${packet.streamIndex}\n"
                        }
                    } else {
                        logText += "No packets found or failed to read any packets.\n"
                    }
                    isLoading = false
                }
            },
            enabled = !isLoading
        ) {
            Text("Read All Video Packets")
        }

        Spacer(modifier = Modifier.height(8.dp))

        Button(
            onClick = {
                logText = "" // Clear log before new operation
                logText += "Decoding next video frame...\n"
                coroutineScope.launch {
                    isLoading = true
                    val frame = withContext(Dispatchers.IO) { parser.decodeNextVideoFrame() }
                    if (frame != null) {
                        logText += "Decoded Frame: Width=${frame.width}, Height=${frame.height}, Format=${frame.format}, PTS=${frame.pts}\n"
                    } else {
                        logText += "No more frames or failed to decode frame.\n"
                    }
                    isLoading = false
                }
            },
            enabled = !isLoading
        ) {
            Text("Decode Next Video Frame")
        }

        Spacer(modifier = Modifier.height(8.dp))

        Button(
            onClick = {
                logText = "" // Clear log before new operation
                logText += "Dumping all frames to YUV (using MediaStore)...\n"
                coroutineScope.launch {
                    isLoading = true
                    var success = false
                    val tempYuvFile = File(context.cacheDir, "temp_output.yuv")
                    val tempYuvPath = tempYuvFile.absolutePath

                    try {
                        // 1. 先将数据倾倒到应用的私有缓存目录
                        val dumpSuccess = withContext(Dispatchers.IO) { parser.dumpAllFramesToYUV(tempYuvPath) }

                        if (dumpSuccess && tempYuvFile.exists()) {
                            // 2. 使用 MediaStore 将文件保存到公共下载目录
                            val contentResolver = context.contentResolver
                            val fileName = "output_${System.currentTimeMillis()}.yuv" // 使用时间戳确保文件名唯一
                            val contentValues = ContentValues().apply {
                                put(MediaStore.MediaColumns.DISPLAY_NAME, fileName)
                                put(MediaStore.MediaColumns.MIME_TYPE, "application/octet-stream") // YUV没有标准MIME类型，使用通用二进制
                                put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS + File.separator + "MyYUVFiles") // 保存到Download/MyYUVFiles/
                                put(MediaStore.MediaColumns.IS_PENDING, 1) // 标记为待处理
                            }

                            val uri = contentResolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, contentValues)

                            if (uri != null) {
                                contentResolver.openOutputStream(uri)?.use { outputStream ->
                                    tempYuvFile.inputStream().copyTo(outputStream)
                                }
                                contentValues.clear()
                                contentValues.put(MediaStore.MediaColumns.IS_PENDING, 0) // 标记为处理完成
                                contentResolver.update(uri, contentValues, null, null)
                                success = true
                                logText += "Successfully dumped all frames to YUV: ${uri.path}\n"
                                Toast.makeText(context, "文件已保存到下载目录: $fileName", Toast.LENGTH_LONG).show()
                            } else {
                                logText += "Failed to create MediaStore entry.\n"
                            }
                        } else {
                            logText += "Failed to dump frames to temporary YUV file.\n"
                        }
                    } catch (e: Exception) {
                        logText += "Error saving file: ${e.message}\n"
                        e.printStackTrace()
                    } finally {
                        // 确保删除临时文件
                        if (tempYuvFile.exists()) {
                            tempYuvFile.delete()
                        }
                        isLoading = false
                    }
                }
            },
            enabled = !isLoading
        ) {
            Text("Dump All Frames to YUV")
        }

        Spacer(modifier = Modifier.height(8.dp))

        Button(
            onClick = {
                logText = "" // Clear log before new operation
                parser.close()
                logText += "MP4 Parser closed.\n"
            },
            enabled = !isLoading
        ) {
            Text("Close MP4 Parser")
        }

        Spacer(modifier = Modifier.height(16.dp))

        Text(
            text = logText,
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f)
                .padding(4.dp)
                .verticalScroll(rememberScrollState())
        )
    }
}


/**
 * 从 assets 复制文件到 app 缓存目录
 * 如果目标文件已存在，则不重复复制
 */
fun copyAssetToCache(context: Context, assetName: String, outFileName: String) {
    val outFile = File(context.cacheDir, outFileName)
    if (outFile.exists()) return

    context.assets.open(assetName).use { inputStream ->
        outFile.outputStream().use { outputStream ->
            inputStream.copyTo(outputStream)
        }
    }
}

@Preview(showBackground = true)
@Composable
fun Mp4ParserScreenPreview() {
    MyApplicationTheme {
        Mp4ParserScreen()
    }
}
