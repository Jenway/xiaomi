package com.example.jnirunner

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.example.jnirunner.ui.theme.JniRunnerTheme
import com.example.mylib
import com.example.mylib.stringToFromJNI

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            JniRunnerTheme {
                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    Column(modifier = Modifier.padding(innerPadding)) {
                        Greeting(
                            name = "Android",
                            modifier = Modifier.padding(innerPadding)
                        )
                        SimpleAdder()
                    }
                }
            }
        }
    }
}

@Composable
fun Greeting(name: String, modifier: Modifier = Modifier) {
    Text(
        text = "Hello $name!",
        modifier = modifier
    )
}

@Preview(showBackground = true)
@Composable
fun GreetingPreview() {
    JniRunnerTheme {
        Greeting("Android")
    }
}


@Composable
fun SimpleAdder() {
    var numA by remember { mutableStateOf("") }
    var numB by remember { mutableStateOf("") }
    var numC by remember { mutableStateOf("") }
    val jniStr = stringToFromJNI()

    Column(modifier = Modifier.padding(16.dp)) {
        Text("调用 String stringToFromJNI() 函数：")
        Spacer(modifier = Modifier.height(16.dp))
        Text(jniStr)

        Text("调用 int add(int a,int b) 函数")
        Spacer(modifier = Modifier.height(16.dp))
        NumReader(value = numA, onValueChange = { numA = it }, label = "数字 A")
        NumReader(value = numB, onValueChange = { numB = it }, label = "数字 B")

        Spacer(modifier = Modifier.height(16.dp))

        Button(onClick = {
            val a = numA.toIntOrNull() ?: 0
            val b = numB.toIntOrNull() ?: 0
            val result = mylib.add(a, b)
            numC = result.toString()
        }) {
            Text("计算 A + B")
        }

        Spacer(modifier = Modifier.height(16.dp))

        Text(text = "结果 C: $numC", style = MaterialTheme.typography.headlineSmall)
    }
}

@Composable
fun NumReader(
    value: String,
    onValueChange: (String) -> Unit,
    label: String
) {
    OutlinedTextField(
        value = value,
        onValueChange = {
            if (it.all { ch -> ch.isDigit() }) onValueChange(it)
        },
        label = { Text(label) },
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
        modifier = Modifier.fillMaxWidth()
    )
}