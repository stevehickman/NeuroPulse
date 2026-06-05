import CoreBluetooth
import Combine

// Central manager for the NeuroPulse hub BLE GATT connection.
// Scans for the hub by service UUID, subscribes to all NOTIFY characteristics,
// parses incoming data, and publishes live state for the rest of the app.
// Also provides write methods for Mode 2 (protocol upload), Mode 4 (EDF request),
// OTA commands, calibration triggers, and SHDR upload confirmation.

final class NeuroPulseGATTManager: NSObject, ObservableObject {

    @Published private(set) var connectionState: ConnectionState = .disconnected
    @Published private(set) var session: SessionState = .empty
    @Published private(set) var zoneModules: [UInt8] = [0, 0, 0, 0, 0]  // one per slot
    @Published private(set) var otaStatus: OTAStatusPacket?
    @Published private(set) var shdrUploadPending = false

    enum ConnectionState { case disconnected, scanning, connecting, connected }

    // Completion handlers for write-with-response operations.
    var onProtocolUploadAck:  ((Result<Void, GATTWriteError>) -> Void)?
    var onEDFRequestAck:      ((Result<Void, GATTWriteError>) -> Void)?
    var onOTACommandAck:      ((Result<Void, GATTWriteError>) -> Void)?
    var onCalibrationAck:     ((Result<Void, GATTWriteError>) -> Void)?

    private var onSessionStopAck: ((Result<Void, GATTWriteError>) -> Void)?

    // MARK: - Private

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?

    // Notify characteristics
    private var sessionStateChar:     CBCharacteristic?
    private var sessionStatusChar:    CBCharacteristic?
    private var hrvCoherenceChar:     CBCharacteristic?
    private var pacerPhaseChar:       CBCharacteristic?
    private var impedanceResultChar:  CBCharacteristic?
    private var consumableStatusChar: CBCharacteristic?
    private var otaStatusChar:        CBCharacteristic?
    private var zoneModuleStatusChar: CBCharacteristic?
    private var shdrUploadStatusChar: CBCharacteristic?

    // Write characteristics
    private var protocolUploadChar:   CBCharacteristic?
    private var edfRequestChar:       CBCharacteristic?
    private var otaCommandChar:       CBCharacteristic?
    private var calibrationCmdChar:   CBCharacteristic?
    private var sessionStopChar:      CBCharacteristic?

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

    // MARK: - Write API

    /// Mode 2: upload a signed session protocol blob to the hub.
    func uploadProtocol(_ blob: Data, completion: @escaping (Result<Void, GATTWriteError>) -> Void) {
        guard let char = protocolUploadChar, let p = peripheral else {
            completion(.failure(.notConnected)); return
        }
        onProtocolUploadAck = completion
        // Hub expects chunks ≤ 512 bytes (BLE 5 max); single write for typical protocol blobs.
        p.writeValue(blob, for: char, type: .withResponse)
    }

    /// Mode 4: request EDF+ download — hub streams data over USB-C when connected.
    func requestEDFDownload(sessionID: UInt32, completion: @escaping (Result<Void, GATTWriteError>) -> Void) {
        guard let char = edfRequestChar, let p = peripheral else {
            completion(.failure(.notConnected)); return
        }
        onEDFRequestAck = completion
        var sid = sessionID
        let data = Data(bytes: &sid, count: 4)
        p.writeValue(data, for: char, type: .withResponse)
    }

    /// Send an OTA opcode (with optional payload for chunk transfers).
    func sendOTACommand(_ opcode: OTAOpcode, payload: Data = Data(),
                        completion: @escaping (Result<Void, GATTWriteError>) -> Void) {
        guard let char = otaCommandChar, let p = peripheral else {
            completion(.failure(.notConnected)); return
        }
        onOTACommandAck = completion
        var buf = Data([opcode.rawValue])
        buf.append(payload)
        p.writeValue(buf, for: char, type: .withResponse)
    }

    /// Send a calibration command.
    func sendCalibration(_ opcode: CalibrationOpcode,
                         completion: @escaping (Result<Void, GATTWriteError>) -> Void) {
        guard let char = calibrationCmdChar, let p = peripheral else {
            completion(.failure(.notConnected)); return
        }
        onCalibrationAck = completion
        p.writeValue(Data([opcode.rawValue]), for: char, type: .withResponse)
    }

    /// Request the hub to stop the active session. Writes a single 0x01 byte.
    /// The app does NOT update session.status here — it waits for the hub to
    /// report the new status over the SESSION_STATUS notify characteristic (ISC-34).
    func sendSessionStop(completion: @escaping (Result<Void, GATTWriteError>) -> Void) {
        guard let char = sessionStopChar, let p = peripheral else {
            completion(.failure(.notConnected)); return
        }
        onSessionStopAck = completion
        p.writeValue(Data([0x01]), for: char, type: .withResponse)
    }
}

// MARK: - Error type

enum GATTWriteError: Error {
    case notConnected
    case peripheralError(Error)
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
        zoneModules = [0, 0, 0, 0, 0]
        clearCharacteristicHandles()
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) { [weak self] in self?.startScan() }
    }

    private func clearCharacteristicHandles() {
        sessionStateChar = nil; sessionStatusChar = nil; hrvCoherenceChar = nil
        pacerPhaseChar = nil; impedanceResultChar = nil; consumableStatusChar = nil
        otaStatusChar = nil; zoneModuleStatusChar = nil; shdrUploadStatusChar = nil
        protocolUploadChar = nil; edfRequestChar = nil; otaCommandChar = nil
        calibrationCmdChar = nil; sessionStopChar = nil
    }
}

// MARK: - CBPeripheralDelegate

extension NeuroPulseGATTManager: CBPeripheralDelegate {

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let service = peripheral.services?.first(where: { $0.uuid == NPUUID.service }) else { return }
        peripheral.discoverCharacteristics(NPUUID.all, for: service)
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
                peripheral.readValue(for: char)
            case NPUUID.otaStatus:
                otaStatusChar = char
                peripheral.setNotifyValue(true, for: char)
            case NPUUID.zoneModuleStatus:
                zoneModuleStatusChar = char
                peripheral.setNotifyValue(true, for: char)
                peripheral.readValue(for: char)
            case NPUUID.shdrUploadStatus:
                shdrUploadStatusChar = char
                peripheral.setNotifyValue(true, for: char)
            case NPUUID.protocolUpload:
                protocolUploadChar = char
            case NPUUID.edfRequest:
                edfRequestChar = char
            case NPUUID.otaCommand:
                otaCommandChar = char
            case NPUUID.calibrationCmd:
                calibrationCmdChar = char
            case NPUUID.sessionStop:
                sessionStopChar = char
            default:
                break
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard error == nil, let data = characteristic.value else { return }

        switch characteristic.uuid {
        case NPUUID.sessionState:
            if let epoch = GATTParser.parseSessionState(data) { pending.epoch = epoch }
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
        case NPUUID.otaStatus:
            otaStatus = GATTParser.parseOTAStatus(data)
        case NPUUID.zoneModuleStatus:
            zoneModules = GATTParser.parseZoneModuleStatus(data) ?? zoneModules
        case NPUUID.shdrUploadStatus:
            // Byte 0: 0x01 = upload requested by hub, 0x02 = upload complete
            shdrUploadPending = data.first == 0x01
        default:
            break
        }

        session = pending
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didWriteValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        let result: Result<Void, GATTWriteError> = error.map { .failure(.peripheralError($0)) } ?? .success(())
        switch characteristic.uuid {
        case NPUUID.protocolUpload:
            onProtocolUploadAck?(result); onProtocolUploadAck = nil
        case NPUUID.edfRequest:
            onEDFRequestAck?(result); onEDFRequestAck = nil
        case NPUUID.otaCommand:
            onOTACommandAck?(result); onOTACommandAck = nil
        case NPUUID.calibrationCmd:
            onCalibrationAck?(result); onCalibrationAck = nil
        case NPUUID.sessionStop:
            onSessionStopAck?(result); onSessionStopAck = nil
        default:
            break
        }
    }
}
