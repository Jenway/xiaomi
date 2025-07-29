package com.example.mathutils

private typealias JniOp<T> = (T, T) -> T

class MathWrapper {
    enum class CalcType { INT, FLOAT, DOUBLE }

    enum class Operator(val symbol: String) {
        ADD("+"), SUB("-"), MUL("×"), DIV("÷");

        companion object {
            fun fromSymbol(symbol: String): Operator? = entries.find { it.symbol == symbol }
        }
    }

    sealed class CalcResult<out T> {
        data class Success<T>(val value: T) : CalcResult<T>()
        data class Error(val message: String) : CalcResult<Nothing>()
    }

    private inline fun <T> String.safeToNumber(convert: (String) -> T): CalcResult<T> =
        runCatching { CalcResult.Success(convert(this)) }.getOrElse { CalcResult.Error("Invalid number format: ${it.message}") }

    private data class CalcOps<T>(
        val add: JniOp<T>, val sub: JniOp<T>, val mul: JniOp<T>, val div: JniOp<T>
    )

    private val intOps = CalcOps(
        add = MathUtils::IntAdd,
        sub = MathUtils::IntSub,
        mul = MathUtils::IntMul,
        div = MathUtils::IntDiv,
    )

    private val floatOps = CalcOps(
        add = MathUtils::FloatAdd,
        sub = MathUtils::FloatSub,
        mul = MathUtils::FloatMul,
        div = MathUtils::FloatDiv,
    )

    private val doubleOps = CalcOps(
        add = MathUtils::DoubleAdd,
        sub = MathUtils::DoubleSub,
        mul = MathUtils::DoubleMul,
        div = MathUtils::DoubleDiv,
    )

    fun calculate(
        a: String, b: String, operator: Operator, calcType: CalcType
    ): CalcResult<Any> {

        val result = when (calcType) {
            CalcType.INT -> performCalculationInternal(a, b, operator, String::toInt, intOps)
            CalcType.FLOAT -> performCalculationInternal(a, b, operator, String::toFloat, floatOps)
            CalcType.DOUBLE -> performCalculationInternal(
                a, b, operator, String::toDouble, doubleOps
            )
        }

        return if (result is CalcResult.Success) CalcResult.Success(result.value as Any)
        else result
    }


    private fun <T : Any> performCalculationInternal(
        aStr: String, bStr: String, operator: Operator, converter: (String) -> T, ops: CalcOps<T>
    ): CalcResult<T> {

        val numX = when (val result = aStr.safeToNumber(converter)) {
            is CalcResult.Success -> result.value
            is CalcResult.Error -> return CalcResult.Error("Invalid input A: ${result.message}")
        }
        val numY = when (val result = bStr.safeToNumber(converter)) {
            is CalcResult.Success -> result.value
            is CalcResult.Error -> return CalcResult.Error("Invalid input B: ${result.message}")
        }

        if (operator == Operator.DIV) {
            when (numY) {
                is Int -> if (numY == 0) return CalcResult.Error("Division by zero error")
                is Float -> if (numY == 0.0f) return CalcResult.Error("Division by zero error")
                is Double -> if (numY == 0.0) return CalcResult.Error("Division by zero error")
            }
        }

        val result = when (operator) {
            Operator.ADD -> ops.add(numX, numY)
            Operator.SUB -> ops.sub(numX, numY)
            Operator.MUL -> ops.mul(numX, numY)
            Operator.DIV -> ops.div(numX, numY)
        }
        return CalcResult.Success(result)
    }
}
