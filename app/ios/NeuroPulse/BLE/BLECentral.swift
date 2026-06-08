//
//  BLECentral.swift
//  NeuroPulse
//
//  Protocol abstraction over CBCentralManager that enables unit testing of
//  NeuroPulseGATTManager without a real Bluetooth stack.
//
//  CBCentralManager already implements every method required by this protocol —
//  the extension at the bottom provides zero-cost conformance.  Tests inject a
//  mock object that satisfies the same interface with a mutable `state` property
//  and call-recording properties.

import CoreBluetooth

/// A minimal capability surface of CBCentralManager needed by NeuroPulseGATTManager.
/// Conforming to a protocol rather than subclassing allows unit tests to inject a
/// lightweight stub without spinning up a real BLE stack.
protocol BLECentralManager: AnyObject {
    /// Current authorisation / power state of the Bluetooth hardware.
    var state: CBManagerState { get }

    /// Begin scanning for peripherals advertising the given service UUIDs.
    func scanForPeripherals(withServices serviceUUIDs: [CBUUID]?, options: [String: Any]?)

    /// Stop an in-progress peripheral scan.
    func stopScan()

    /// Initiate a connection to the discovered peripheral.
    func connect(_ peripheral: CBPeripheral, options: [String: Any]?)

    /// Cancel an active or pending connection.
    func cancelPeripheralConnection(_ peripheral: CBPeripheral)
}

/// CBCentralManager satisfies BLECentralManager without any code changes.
/// This conformance is declared here rather than in NeuroPulseGATTManager.swift
/// so the protocol file is self-contained.
extension CBCentralManager: BLECentralManager {}
