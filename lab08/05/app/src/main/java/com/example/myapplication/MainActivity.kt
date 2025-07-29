package com.example.myapplication

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.material3.HorizontalDivider
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import com.example.mathutils.MathWrapper
import com.example.myapplication.ui.theme.MyApplicationTheme

private val mathWrapper = MathWrapper()

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            MyApplicationTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    CalculatorScreen()
                }
            }
        }
    }
}

@Composable
fun CalculatorScreen() {
    var inputA by remember { mutableStateOf("") }
    var inputB by remember { mutableStateOf("") }
    var result by remember { mutableStateOf("等待输入并点击运算") }

    val types = listOf("Int", "Float", "Double")
    val operators = listOf("+", "-", "×", "÷")

    var selectedType by remember { mutableStateOf(types[0]) }
    var selectedOperator by remember { mutableStateOf(operators[0]) }

    Column(
        modifier = Modifier
            .padding(24.dp)
            .fillMaxSize(),
        verticalArrangement = Arrangement.spacedBy(20.dp)
    ) {
        Text("JNI 计算器", style = MaterialTheme.typography.headlineMedium)

        // 输入与操作符
        Row(
            horizontalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()
        ) {
            OutlinedTextField(
                value = inputA,
                onValueChange = { inputA = it },
                label = { Text("A") },
                keyboardOptions = KeyboardOptions.Default.copy(keyboardType = KeyboardType.Number),
                modifier = Modifier.weight(2f)
            )
            OperatorDropdown(
                selectedOperator = selectedOperator,
                onOperatorSelected = { selectedOperator = it },
                modifier = Modifier.weight(1f)
            )
            OutlinedTextField(
                value = inputB,
                onValueChange = { inputB = it },
                label = { Text("B") },
                keyboardOptions = KeyboardOptions.Default.copy(keyboardType = KeyboardType.Number),
                modifier = Modifier.weight(2f)
            )
        }

        // 类型选择
        Row(
            horizontalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()
        ) {
            types.forEach { type ->
                ElevatedButton(
                    onClick = { selectedType = type },
                    colors = if (selectedType == type) ButtonDefaults.buttonColors()
                    else ButtonDefaults.elevatedButtonColors(),
                    modifier = Modifier.weight(1f)
                ) {
                    Text(type)
                }
            }
        }

        // 计算按钮
        Button(
            onClick = {
                result = calculate(inputA, inputB, selectedOperator, selectedType)
            }, modifier = Modifier.fillMaxWidth()
        ) {
            Text("计算")
        }

        HorizontalDivider(Modifier, DividerDefaults.Thickness, DividerDefaults.color)

        // 输出结果
        Text(
            text = result,
            style = MaterialTheme.typography.bodyLarge,
            modifier = Modifier.padding(top = 12.dp)
        )
    }
}

@Composable
fun OperatorDropdown(
    selectedOperator: String, onOperatorSelected: (String) -> Unit, modifier: Modifier = Modifier
) {
    var expanded by remember { mutableStateOf(false) }
    val operators = listOf("+", "-", "×", "÷")

    Box(modifier = modifier) {
        OutlinedButton(
            onClick = { expanded = true }, modifier = Modifier.fillMaxWidth()
        ) {
            Text(selectedOperator)
        }
        DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
            operators.forEach { op ->
                DropdownMenuItem(text = { Text(op) }, onClick = {
                    onOperatorSelected(op)
                    expanded = false
                })
            }
        }
    }
}

fun calculate(a: String, b: String, op: String, type: String): String {
    val mathWrapper = MathWrapper()

    val operator = MathWrapper.Operator.fromSymbol(op) ?: return "未知操作: $op"

    val calcType = when (type) {
        "Int" -> MathWrapper.CalcType.INT
        "Float" -> MathWrapper.CalcType.FLOAT
        "Double" -> MathWrapper.CalcType.DOUBLE
        else -> return "类型错误: $type"
    }

    return when (val result = mathWrapper.calculate(a, b, operator, calcType)) {
        is MathWrapper.CalcResult.Success -> "结果: ${result.value}"
        is MathWrapper.CalcResult.Error -> "错误: ${result.message}"
    }
}