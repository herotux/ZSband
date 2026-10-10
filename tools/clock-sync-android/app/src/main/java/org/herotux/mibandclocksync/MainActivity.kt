package org.herotux.mibandclocksync

import android.Manifest
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.ParcelUuid
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.TimeZone

class MainActivity : Activity() {
    private val serviceUuid = ParcelUuid.fromString("8d2f7b10-6d3a-4b19-9e2a-73c4d5a6b701")
    private val timeUuid = java.util.UUID.fromString("8d2f7b11-6d3a-4b19-9e2a-73c4d5a6b701")
    private lateinit var status: TextView
    private lateinit var syncButton: Button
    private var gatt: BluetoothGatt? = null
    private var scanning = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(24, 40, 24, 24)
        }
        val title = TextView(this).apply {
            text = "همگام‌سازی ساعت شمسی"
            textSize = 24f
            gravity = Gravity.CENTER
        }
        status = TextView(this).apply {
            text = "دستبند را نزدیک گوشی نگه دارید و دکمهٔ همگام‌سازی را بزنید."
            textSize = 16f
            gravity = Gravity.CENTER
            setPadding(0, 24, 0, 24)
        }
        syncButton = Button(this).apply {
            text = "همگام‌سازی زمان"
            setOnClickListener { startSync() }
        }
        root.addView(title, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
        root.addView(status, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
        root.addView(syncButton, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
        setContentView(ScrollView(this).apply { addView(root) })
    }

    private fun requiredPermissions(): Array<String> {
        return if (Build.VERSION.SDK_INT >= 31) {
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }
    }

    private fun startSync() {
        val missing = requiredPermissions().filter {
            checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED
        }
        if (missing.isNotEmpty()) {
            requestPermissions(missing.toTypedArray(), 42)
            return
        }
        val adapter = (getSystemService(Context.BLUETOOTH_SERVICE) as android.bluetooth.BluetoothManager).adapter
        if (adapter == null || !adapter.isEnabled) {
            status.text = "بلوتوث گوشی را روشن کنید."
            return
        }
        gatt?.close()
        gatt = null
        syncButton.isEnabled = false
        status.text = "در حال جست‌وجوی Mi Band 4 Clock…"
        val scanner = adapter.bluetoothLeScanner
        val filters = listOf(ScanFilter.Builder().setServiceUuid(serviceUuid).build())
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        scanning = true
        scanner.startScan(filters, settings, scanCallback)
        window.decorView.postDelayed({
            if (scanning) {
                stopScan()
                syncButton.isEnabled = true
                status.text = "دستبند پیدا نشد. مطمئن شوید فریمور ساعت شمسی اجراست."
            }
        }, 15_000)
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            if (!scanning) return
            stopScan()
            status.text = "دستبند پیدا شد؛ در حال اتصال…"
            gatt = result.device.connectGatt(this@MainActivity, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        }

        override fun onScanFailed(errorCode: Int) {
            stopScan()
            syncButton.isEnabled = true
            status.text = "خطای جست‌وجوی بلوتوث: $errorCode"
        }
    }

    private fun stopScan() {
        if (!scanning) return
        scanning = false
        try {
            val adapter = (getSystemService(Context.BLUETOOTH_SERVICE) as android.bluetooth.BluetoothManager).adapter
            adapter.bluetoothLeScanner?.stopScan(scanCallback)
        } catch (_: SecurityException) {
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, statusCode: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                runOnUiThread { status.text = "متصل شد؛ در حال آماده‌سازی همگام‌سازی…" }
                g.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                runOnUiThread {
                    syncButton.isEnabled = true
                    if (statusCode == BluetoothGatt.GATT_SUCCESS) {
                        status.text = "همگام‌سازی انجام شد."
                    } else {
                        status.text = "اتصال قطع شد (کد $statusCode). دوباره تلاش کنید."
                    }
                }
                g.close()
                if (gatt === g) gatt = null
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, statusCode: Int) {
            if (statusCode != BluetoothGatt.GATT_SUCCESS) {
                fail(g, "خواندن سرویس بلوتوث ناموفق بود.")
                return
            }
            val characteristic = g.getService(serviceUuid.uuid)?.getCharacteristic(timeUuid)
            if (characteristic == null) {
                fail(g, "سرویس همگام‌سازی ساعت در دستبند پیدا نشد.")
                return
            }

            val nowMillis = System.currentTimeMillis()
            val utcEpochSeconds = nowMillis / 1000L
            val offsetMinutes = TimeZone.getDefault().getOffset(nowMillis) / 60_000
            val payload = ByteBuffer.allocate(10).order(ByteOrder.LITTLE_ENDIAN)
                .putLong(utcEpochSeconds)
                .putShort(offsetMinutes.toShort())
                .array()

            val started = if (Build.VERSION.SDK_INT >= 33) {
                g.writeCharacteristic(characteristic, payload, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) ==
                    BluetoothStatusCodes.SUCCESS
            } else {
                @Suppress("DEPRECATION")
                characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                @Suppress("DEPRECATION")
                characteristic.value = payload
                @Suppress("DEPRECATION")
                g.writeCharacteristic(characteristic)
            }
            if (!started) fail(g, "ارسال زمان به دستبند شروع نشد.")
        }

        override fun onCharacteristicWrite(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            statusCode: Int
        ) {
            runOnUiThread {
                if (statusCode == BluetoothGatt.GATT_SUCCESS) {
                    status.text = "زمان گوشی با موفقیت ارسال شد. ساعت و تاریخ شمسی باید به‌روز شده باشند."
                    syncButton.isEnabled = true
                    g.disconnect()
                } else {
                    status.text = "ارسال زمان ناموفق بود: $statusCode"
                    syncButton.isEnabled = true
                    g.disconnect()
                }
            }
        }
    }

    private fun fail(g: BluetoothGatt, message: String) {
        runOnUiThread {
            status.text = message
            syncButton.isEnabled = true
        }
        g.disconnect()
    }

    override fun onRequestPermissionsResult(
        requestCode: Int, permissions: Array<out String>, grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 42 && grantResults.isNotEmpty() &&
            grantResults.all { it == PackageManager.PERMISSION_GRANTED }) {
            startSync()
        } else {
            status.text = "برای همگام‌سازی، مجوز بلوتوث لازم است."
            syncButton.isEnabled = true
        }
    }

    override fun onDestroy() {
        stopScan()
        gatt?.close()
        gatt = null
        super.onDestroy()
    }
}
