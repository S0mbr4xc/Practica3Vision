package com.s0mbr4xc.practica3vision

import android.graphics.Bitmap
import android.os.Bundle
import android.content.res.AssetManager
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.Spinner
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {

    // JNI bindings
    external fun initAssetManager(assetManager: AssetManager)
    external fun detectShapeFromBitmap(bitmap: Bitmap, descriptor: Int): String

    companion object {
        init {
            System.loadLibrary("practica3vision")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // preparar acceso a assets en la capa nativa
        initAssetManager(assets)

        setContentView(R.layout.activity_main)

        val drawingView = findViewById<DrawingView>(R.id.drawingView)
        val btnDetect = findViewById<Button>(R.id.btnDetect)
        val btnClear = findViewById<Button>(R.id.btnClear)
        val spinner = findViewById<Spinner>(R.id.spinnerDescriptors)

        // Inicializar spinner con las opciones del strings.xml
        ArrayAdapter.createFromResource(
            this,
            R.array.descriptors_array,
            android.R.layout.simple_spinner_item
        ).also { adapter ->
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
            spinner.adapter = adapter
        }

        btnDetect.setOnClickListener {
            val descriptor = spinner.selectedItemPosition // 0 = Hu, 1 = FFT
            val bmp = drawingView.getDrawingBitmap()
            val result = detectShapeFromBitmap(bmp, descriptor)
            Toast.makeText(this, "Figura detectada: $result", Toast.LENGTH_LONG).show()
        }

        btnClear.setOnClickListener {
            drawingView.clearCanvas()
        }
    }
}