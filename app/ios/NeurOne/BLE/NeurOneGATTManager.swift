//
//  NeurOneGATTManager.swift
//  NeurOne
//
//  Central manager for the NeurOne hub BLE GATT connection.
//  Scans for the hub by service UUID, subscribes to all NOTIFY characteristics,
//  parses incoming data, and publishes live state for the rest of the app.
//  Also provides write methods for Mode 2 (protocol upload), Mode 4 (EDF request),
//  OTA commands, calibration triggers, SHDR upload confirmation, and session stop.
//
//  Unit-testing design
//  ────────────────────
//  CBCentralManager cannot be instantiated in unit tests (it requires a real BLE stack).
//  This class accepts an injected BLECentralManager via init(mockCentral:) so tests can
//  drive state without hardware.
//
//  State transitions are extracted into internal "apply" methods:
//    • applyStateUpdate(state:)          — handles BLE power/auth changes
//    • applyCharacteristicAssignment(discovered:) — marks all chars resolved
//    • applyDisconnection()              — tears down session state and schedules reconnect
//    • applyWarrantyToken(_:)            — stores/validates the 32-byte TRNG hub token
//
//  The CBCentralManagerDelegate and CBPeripheralDelegate callbacks call these same methods,
//  so production and test code share a single code path.

import CoreBluetooth
import Combine
import OSLog

@MainActor
final class NeurOneGATTManager: NSObject, ObservableObject {

    private static let logger = Logger(subsystem: "life.neurone.app", category: "BLE")

    // MARK: - Published state

    /// Current lifecycle state of the BLE connection to the hub.
    @Published private(set) var connectionState: ConnectionState = .disconnected

    /// Live session data decoded from hub NOTIFY characteristics.
    @Published private(set) var session: SessionState = .empty

    /// Zone module presence — one byte per slot (0 = absent, 1–5 = zone ID confirmed).
    @Published private(set) var zoneModules: [UInt8] = [0, 0, 0, 0, 0]

    /// Latest OTA progress/error packet from the hub.
    @Published private(set) var otaStatus: OTAStatusPacket?

    /// True while the hub has requested a SHDR fleet upload.
    @Published private(set) var shdrUploadPending = false

    /// True when BLE is powered off, unauthorized, or unsupported.
    /// SessionView uses this to show an "Enable Bluetooth" prompt (ISC-19).
    @Published private(set) var bluetoothUnavailable = false

    /// True once all 14 required GATT characteristics are discovered and assigned.
    /// SessionView gates session-start UI on this flag (ISC-13).
    @Published private(set) var allCharacteristicsResolved = false

    /// 256-bit TRNG warranty token provisioned by the hub at first pairing (ISC-SHDR-TRNG).
    /// Delivered over the GATT `warrantyToken` characteristic (READ 32B).
    /// Cleared on disconnect; nil until the hub provisions it via GATT.
    /// SHDRUploader observes this property and upgrades from its Keychain fallback when non-nil
    /// (NP-FW-EMMC-002 Rev A §A, OI-WA-03).
    @Published private(set) var warrantyToken: Data?

    /// Current hub firmware version decoded from FIRMWARE_VERSION characteristic (ISC-108).
    /// Encoded as uint32 little-endian — bits [23:16]=major [15:8]=minor [7:0]=patch.
    /// nil until the hub ships NPUUID.firmwareVersion (OI-WA-03); OTAView shows "Unknown".
    @Published private(set) var hubFirmwareVersion: String?

    // MARK: - Connection lifecycle type

    enum ConnectionState { case disconnected, scanning, connecting, connected }

    // MARK: - Write-operation completion handlers

    var onProtocolUploadAck: ((Result<Void, GATTWriteError>) -> Void)?
    var onEDFRequestAck:     ((Result<Void, GATTWriteError>) -> Void)?
    var onOTACommandAck:     ((Result<Void, GATTWriteError>) -> Void)?
    var onCalibrationAck:    ((Result<Void, GATTWriteError>) -> Void)?

    private var onSessionStopAck: ((Result<Void, GATTWriteError>) -> Void)?

    // MARK: - Private properties

    /// Injected BLE central — CBCentralManager in production; MockBLECentral in tests.
    var central: BLECentralManager!                 // `internal` — accessed by tests

    private var peripheral: CBPeripheral?

    // Notify/read characteristics
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
    private var protocolUploadChar: CBCharacteristic?
    private var edfRequestChar:     CBCharacteristic?
    private var otaCommandChar:     CBCharacteristic?
    private var calibrationCmdChar: CBCharacteristic?
    private var sessionStopChar:    CBCharacteristic?

    // Dedicated subject for impedance results — fires ONLY from NPUUID.impedanceResult handler.
    // Distinct from session state updates so SetupGATTProviding.waitForImpedanceResult cannot
    // false-trigger on unrelated session emissions (HRV, pacer phase, etc.).
    private let impedanceResultSubject = PassthroughSubject<UInt16, Never>()

    // Optional — not in NPUUID.all; hub firmware pending (OI-WA-03).
    private var warrantyTokenChar:    CBCharacteristic?
    private var firmwareVersionChar:  CBCharacteristic?

    /// In-flight partial session state accumulated from individual characteristic notifications.
    private var pending: SessionState = .empty

    // MARK: - Initialisers

    /// Production initialiser — creates a real CBCentralManager on the main queue.
    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main,
                                   options: [CBCentralManagerOptionShowPowerAlertKey: true])
    }

    /// Testing initialiser — accepts a mock BLE central so unit tests can drive state directly
    /// without a real Bluetooth stack.  Do not use in production code.
    init(mockCentral: BLECentralManager) {
        super.init()
        central = mockCentral
        // Delegate is not set on mockCentral — tests drive state via the apply* methods.
    }

    // MARK: - Internal state-machine methods (called by both delegates AND tests)

    /// Handle a BLE power/authorization state change.
    /// Sets `bluetoothUnavailable`, resets `connectionState`, and starts scanning when ready.
    func applyStateUpdate(state: CBManagerState) {
        switch state {
        case .poweredOn:
            bluetoothUnavailable = false
            startScan()
        default:
            bluetoothUnavailable = true
            // applyDisconnection clears stale session/UHDR state and cancels the reconnect
            // timer (guard blocks because central.state != .poweredOn at this point).
            // This prevents stale HRV, impedance, or coherence values from surviving a
            // transient BLE .resetting event and appearing on reconnect.
            applyDisconnection()
        }
    }

    /// Record which GATT characteristic UUIDs were discovered for this connection.
    /// Sets `allCharacteristicsResolved` to true when every UUID in NPUUID.all is present.
    ///
    /// PRIVACY NOTE: `discovered` is a device capability fingerprint (SHDR-class).
    /// It must never be logged, persisted, or transmitted — only the boolean result
    /// (`allCharacteristicsResolved`) leaves this method.
    func applyCharacteristicAssignment(discovered: Set<CBUUID>) {
        allCharacteristicsResolved = NPUUID.all.allSatisfy { discovered.contains($0) }
    }

    /// Tear down active-session state after a disconnect and schedule a reconnect scan
    /// after 2 seconds — but only when BLE is still available.
    func applyDisconnection() {
        connectionState = .disconnected
        peripheral = nil
        session = .empty
        pending = .empty
        zoneModules = [0, 0, 0, 0, 0]
        allCharacteristicsResolved = false
        warrantyToken = nil
        hubFirmwareVersion = nil
        clearCharacteristicHandles()

        // Guard: no reconnect timer when BLE is unavailable — avoids a silent no-op scan.
        guard central.state == .poweredOn else { return }
        Task { [weak self] in
            try? await Task.sleep(nanoseconds: 2_000_000_000)
            self?.startScan()
        }
    }

    /// Store a hub-provisioned TRNG warranty token.
    /// The hub GATT characteristic carries exactly 32 bytes — payloads shorter than 32 bytes
    /// are silently rejected (guard against corrupt or partial GATT reads).
    /// Payloads longer than 32 bytes are truncated to the first 32 bytes (future extension).
    func applyWarrantyToken(_ data: Data) {
        guard data.count >= 32 else { return }
        warrantyToken = data.prefix(32)
    }

    /// Store a hub-reported firmware version from FIRMWARE_VERSION characteristic.
    /// Called by CBPeripheralDelegate (data path) and by tests (applyFirmwareVersion).
    func applyFirmwareVersion(_ version: FirmwareVersion) {
        hubFirmwareVersion = version.description
    }

    /// Set connection state to .connected without real BLE hardware — test use only.
    /// Production path: centralManager(_:didConnect:) in CBCentralManagerDelegate.
    func applyConnected() {
        connectionState = .connected
    }

    // MARK: - Scan / connection helpers

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

    /// Mode 2: write a single protocol chunk to the hub (called per-chunk by SessionProtocolUploader).
    func uploadProtocol(_ chunk: Data, completion: @escaping (Result<Void, GATTWriteError>) -> Void) {
        guard let char = protocolUploadChar, let p = peripheral else {
            completion(.failure(.notConnected)); return
        }
        onProtocolUploadAck = completion
        p.writeValue(chunk, for: char, type: .withResponse)
    }

    /// Mode 4: request EDF+ download — hub streams data over USB-C when connected.
    func requestEDFDownload(sessionID: UInt32,
                            completion: @escaping (Result<Void, GATTWriteError>) -> Void) {
        guard let char = edfRequestChar, let p = peripheral else {
            completion(.failure(.notConnected)); return
        }
        onEDFRequestAck = completion
        // Explicit little-endian serialization per hub wire contract (all GATT frames are LE).
        // `sessionID.littleEndian` is a no-op on ARM64 but makes the intent unambiguous and
        // safe on any host byte order.
        var sid = sessionID.littleEndian
        p.writeValue(Data(bytes: &sid, count: 4), for: char, type: .withResponse)
    }

    /// Send an OTA opcode with an optional chunk payload.
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

    /// Send a calibration trigger command.
    func sendCalibration(_ opcode: CalibrationOpcode,
                         completion: @escaping (Result<Void, GATTWriteError>) -> Void) {
        guard let char = calibrationCmdChar, let p = peripheral else {
            completion(.failure(.notConnected)); return
        }
        onCalibrationAck = completion
        p.writeValue(Data([opcode.rawValue]), for: char, type: .withResponse)
    }

    /// Request the hub to stop the active session (writes a single 0x01 byte).
    /// The app does NOT update session.status here — it waits for the hub to report
    /// the new status over SESSION_STATUS NOTIFY (ISC-34).
    func sendSessionStop(completion: @escaping (Result<Void, GATTWriteError>) -> Void) {
        guard let char = sessionStopChar, let p = peripheral else {
            completion(.failure(.notConnected)); return
        }
        onSessionStopAck = completion
        p.writeValue(Data([0x01]), for: char, type: .withResponse)
    }

    // MARK: - Private helpers

    private func clearCharacteristicHandles() {
        sessionStateChar = nil;   sessionStatusChar = nil; hrvCoherenceChar = nil
        pacerPhaseChar = nil;     impedanceResultChar = nil; consumableStatusChar = nil
        otaStatusChar = nil;      zoneModuleStatusChar = nil; shdrUploadStatusChar = nil
        protocolUploadChar = nil; edfRequestChar = nil; otaCommandChar = nil
        calibrationCmdChar = nil; sessionStopChar = nil; warrantyTokenChar = nil
        firmwareVersionChar = nil
    }
}

// MARK: - Error type

enum GATTWriteError: Error {
    case notConnected
    case peripheralError(Error)
}

// MARK: - CBCentralManagerDelegate

extension NeurOneGATTManager: @preconcurrency CBCentralManagerDelegate {

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        applyStateUpdate(state: central.state)
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
        if let error {
            Self.logger.error("Hub disconnected: \(String(describing: error), privacy: .public)")
        }
        applyDisconnection()
    }
}

// MARK: - CBPeripheralDelegate

extension NeurOneGATTManager: @preconcurrency CBPeripheralDelegate {

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let service = peripheral.services?.first(where: { $0.uuid == NPUUID.service })
        else { return }
        // Discover required chars plus optional warrantyToken and firmwareVersion (OI-WA-03).
        peripheral.discoverCharacteristics(
            NPUUID.all + [NPUUID.warrantyToken, NPUUID.firmwareVersion], for: service)
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
                // ISC-20: read current value immediately so reconnects restore in-flight
                // session state without waiting for the next NOTIFY update.
                peripheral.readValue(for: char)

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

            case NPUUID.warrantyToken:
                // Optional — hub firmware not yet shipped. Silently absent until OI-WA-03 lands.
                warrantyTokenChar = char
                peripheral.readValue(for: char)

            case NPUUID.firmwareVersion:
                // Optional — hub firmware not yet shipped (OI-WA-03).
                firmwareVersionChar = char
                peripheral.setNotifyValue(true, for: char)
                peripheral.readValue(for: char)

            default:
                break
            }
        }

        // Compute which required UUIDs were discovered and publish the resolved state.
        // warrantyToken is intentionally excluded from NPUUID.all, so its absence never
        // blocks allCharacteristicsResolved (ISC-13).
        let discovered = Set(service.characteristics?.map { $0.uuid } ?? [])
        applyCharacteristicAssignment(discovered: discovered)
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard error == nil, let data = characteristic.value else { return }

        // ── SHDR characteristics ─────────────────────────────────────────────────
        // These update device-level properties only. They exit here and NEVER reach
        // `session = pending` below, making the UHDR/SHDR boundary structurally
        // enforced rather than convention-dependent.

        if characteristic.uuid == NPUUID.warrantyToken {
            // Hub-provisioned TRNG token — SHDR-linked device identity.
            // applyWarrantyToken rejects payloads shorter than 32 bytes (NP-FW-EMMC-002 Rev A §A).
            applyWarrantyToken(data)
            return
        }

        if characteristic.uuid == NPUUID.firmwareVersion {
            // Current hub firmware version — SHDR-class device metric, never user biology.
            if let version = GATTParser.parseFirmwareVersion(data) {
                applyFirmwareVersion(version)
            }
            return
        }

        if characteristic.uuid == NPUUID.shdrUploadStatus {
            // 0x01 = hub requests SHDR fleet upload; 0x02 = upload acknowledged.
            shdrUploadPending = data.first == 0x01
            return
        }

        // ── UHDR session characteristics ─────────────────────────────────────────
        // All cases below accumulate into `pending` (user health data) and publish
        // via `session = pending` at the end.

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
                impedanceResultSubject.send(flags)
            }

        case NPUUID.consumableStatus:
            if let counts = GATTParser.parseConsumableStatus(data) {
                pending.consumableSessionCounts = counts
            }

        case NPUUID.otaStatus:
            otaStatus = GATTParser.parseOTAStatus(data)

        case NPUUID.zoneModuleStatus:
            zoneModules = GATTParser.parseZoneModuleStatus(data) ?? zoneModules

        default:
            break
        }

        session = pending
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didWriteValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        let result: Result<Void, GATTWriteError> =
            error.map { .failure(.peripheralError($0)) } ?? .success(())

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

// MARK: - SetupGATTProviding conformance

extension NeurOneGATTManager: SetupGATTProviding {
    var sessionPublisher: AnyPublisher<SessionState, Never> {
        $session.eraseToAnyPublisher()
    }
    var zoneModulesPublisher: AnyPublisher<[UInt8], Never> {
        $zoneModules.eraseToAnyPublisher()
    }
    var impedanceResultPublisher: AnyPublisher<UInt16, Never> {
        impedanceResultSubject.eraseToAnyPublisher()
    }
}
