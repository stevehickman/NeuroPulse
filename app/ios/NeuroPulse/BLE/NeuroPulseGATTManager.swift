import CoreBluetooth
import Combine

// Central manager for the NeuroPulse hub BLE GATT connection.
// Scans for the hub by service UUID, subscribes to all NOTIFY characteristics,
// parses incoming data, and publishes a live SessionState for the rest of the app.

final class NeuroPulseGATTManager: NSObject, ObservableObject {

    // Published state — observed by PhoneSessionManager and SwiftUI views.
    @Published private(set) var connectionState: ConnectionState = .disconnected
    @Published private(set) var session: SessionState = .empty

    enum ConnectionState { case disconnected, scanning, connecting, connected }

    // MARK: - Private

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?

    // Characteristic handles — populated on discovery.
    private var sessionStateChar:     CBCharacteristic?
    private var sessionStatusChar:    CBCharacteristic?
    private var hrvCoherenceChar:     CBCharacteristic?
    private var pacerPhaseChar:       CBCharacteristic?
    private var impedanceResultChar:  CBCharacteristic?
    private var consumableStatusChar: CBCharacteristic?

    // Working mutable copy updated by each notification, then published atomically.
    private var pending: SessionState = .empty

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main,
                                   options: [CBCentralManagerOptionShowPowerAlertKey: true])
    }

    func startScan() {
        guard central.state == .poweredOn else { return }
        connectionState = .scanning
        central.scanForPeripherals(withServices: [NPUUID.service], options: nil)
    }

    func disconnect() {
        guard let p = peripheral else { return }
        central.cancelPeripheralConnection(p)
    }
}

// MARK: - CBCentralManagerDelegate

extension NeuroPulseGATTManager: CBCentralManagerDelegate {

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn { startScan() }
    }

    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        self.peripheral = peripheral
        central.stopScan()
        connectionState = .connecting
        central.connect(peripheral, options: nil)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        connectionState = .connected
        peripheral.delegate = self
        peripheral.discoverServices([NPUUID.service])
    }

    func centralManager(_ central: CBCentralManager,
                        didDisconnectPeripheral peripheral: CBPeripheral,
                        error: Error?) {
        connectionState = .disconnected
        self.peripheral = nil
        session = .empty
        // Auto-rescan after hub disconnect.
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) { [weak self] in self?.startScan() }
    }
}

// MARK: - CBPeripheralDelegate

extension NeuroPulseGATTManager: CBPeripheralDelegate {

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let service = peripheral.services?.first(where: { $0.uuid == NPUUID.service }) else { return }
        peripheral.discoverCharacteristics([
            NPUUID.sessionState, NPUUID.sessionStatus, NPUUID.hrvCoherence,
            NPUUID.pacerPhase, NPUUID.impedanceResult, NPUUID.consumableStatus
        ], for: service)
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
        service.characteristics?.forEach { char in
            switch char.uuid {
            case NPUUID.sessionState:
                sessionStateChar = char
                peripheral.setNotifyValue(true, for: char)
            case NPUUID.sessionStatus:
                sessionStatusChar = char
                peripheral.setNotifyValue(true, for: char)
            case NPUUID.hrvCoherence:
                hrvCoherenceChar = char
                peripheral.setNotifyValue(true, for: char)
            case NPUUID.pacerPhase:
                pacerPhaseChar = char
                peripheral.setNotifyValue(true, for: char)
            case NPUUID.impedanceResult:
                impedanceResultChar = char
                peripheral.setNotifyValue(true, for: char)
            case NPUUID.consumableStatus:
                consumableStatusChar = char
                peripheral.setNotifyValue(true, for: char)
                peripheral.readValue(for: char)       // initial read; subsequent updates via NOTIFY
            default: break
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard error == nil, let data = characteristic.value else { return }

        switch characteristic.uuid {
        case NPUUID.sessionState:
            if let epoch = GATTParser.parseSessionState(data) {
                pending.epoch = epoch
            }
        case NPUUID.sessionStatus:
            if let (pid, status) = GATTParser.parseSessionStatus(data) {
                pending.protocolID = pid
                pending.status = status
            }
        case NPUUID.hrvCoherence:
            pending.hrv = GATTParser.parseHRVCoherence(data)
        case NPUUID.pacerPhase:
            if let (phase, pct) = GATTParser.parsePacerPhase(data) {
                pending.pacerPhase = phase
                pending.pacerElapsedPercent = pct
            }
        case NPUUID.impedanceResult:
            if let flags = GATTParser.parseImpedanceResult(data) {
                pending.impedancePassFlags = flags
            }
        case NPUUID.consumableStatus:
            if let counts = GATTParser.parseConsumableStatus(data) {
                pending.consumableSessionCounts = counts
            }
        default: break
        }

        // Publish updated state after every notification — main queue already.
        session = pending
    }
}
