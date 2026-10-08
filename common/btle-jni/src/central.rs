//! The Bluetooth LE central: btleplug driven by a command queue, reporting through an event queue.
//!
//! Nothing here knows about the JVM. The Kotlin side (`DesktopBleCentral`) sends [`Command`]s and
//! polls [`Event`]s, and the pair mirrors `life.neurone.shared.ble.BleCentral` and
//! `BleCentralListener` one for one, so the hub-link state machine above it is the same code that
//! runs on Android, iOS and the browser.
//!
//! Ordering: scan, connect and disconnect run on the actor and never block it; every GATT
//! operation (discover, subscribe, read, write) goes through one worker in the order it was
//! queued, because a protocol upload is a run of writes that must reach the hub in order.

use btleplug::api::{
    Central as _, CentralEvent, CentralState, Characteristic, Manager as _, Peripheral as _,
    ScanFilter, WriteType,
};
use btleplug::platform::{Adapter, Manager, Peripheral};
use futures::stream::{Stream, StreamExt};
use std::collections::{HashMap, HashSet};
use std::pin::Pin;
use std::sync::{mpsc, Arc, Mutex};
use std::time::Duration;
use tokio::sync::mpsc::{unbounded_channel, UnboundedReceiver, UnboundedSender};
use tokio::task::JoinHandle;
use uuid::Uuid;

const CONNECT_TIMEOUT: Duration = Duration::from_secs(15);
const STATE_POLL: Duration = Duration::from_secs(2);

/// What the app asks of the radio. Mirrors `BleCentral`.
#[derive(Debug, Clone, PartialEq)]
pub enum Command {
    StartScan(Uuid),
    StopScan,
    Connect(String),
    Disconnect,
    Discover { service: Uuid, wanted: Vec<Uuid> },
    EnableNotifications(Uuid),
    Read(Uuid),
    Write(Uuid, Vec<u8>),
    Refresh,
}

/// Ordinals match `life.neurone.shared.ble.AdapterState`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
// btleplug reports no "unauthorized" (a denied macOS permission reads as Unknown), but the code is
// part of the Kotlin enum's ordinals, so it stays.
#[allow(dead_code)]
pub enum AdapterCode {
    Unknown = 0,
    Off = 1,
    Unauthorized = 2,
    On = 3,
}

/// What the radio reports. Mirrors `BleCentralListener`.
#[derive(Debug, Clone, PartialEq)]
pub enum Event {
    AdapterState(AdapterCode),
    DeviceFound(String),
    Connected(String),
    Disconnected(String),
    CharacteristicsDiscovered(Vec<Uuid>),
    CharacteristicChanged(Uuid, Vec<u8>),
    CharacteristicRead(Uuid, Vec<u8>),
    /// A read the link refused. On an encrypted characteristic of an unpaired link this is the pairing signal (`OI-UI-KMP-03`).
    CharacteristicReadFailed(Uuid),
}

impl Event {
    /// The wire form read by `DesktopBleCentral.decode`: one kind byte, then
    /// 1 `[state]` · 2/3/4 `[utf-8 device id]` · 5 `[16-byte uuid]*` · 6/7 `[16-byte uuid][value]` · 8 `[16-byte uuid]`.
    pub fn encode(&self) -> Vec<u8> {
        match self {
            Event::AdapterState(s) => vec![1, *s as u8],
            Event::DeviceFound(id) => tagged(2, id.as_bytes()),
            Event::Connected(id) => tagged(3, id.as_bytes()),
            Event::Disconnected(id) => tagged(4, id.as_bytes()),
            Event::CharacteristicsDiscovered(uuids) => {
                let mut out = vec![5];
                for u in uuids {
                    out.extend_from_slice(u.as_bytes());
                }
                out
            }
            Event::CharacteristicChanged(u, v) => with_uuid(6, u, v),
            Event::CharacteristicRead(u, v) => with_uuid(7, u, v),
            Event::CharacteristicReadFailed(u) => with_uuid(8, u, &[]),
        }
    }
}

fn tagged(kind: u8, body: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(1 + body.len());
    out.push(kind);
    out.extend_from_slice(body);
    out
}

fn with_uuid(kind: u8, uuid: &Uuid, value: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(17 + value.len());
    out.push(kind);
    out.extend_from_slice(uuid.as_bytes());
    out.extend_from_slice(value);
    out
}

/// A running central: a runtime, the command sender and the event receiver.
pub struct Central {
    commands: UnboundedSender<Command>,
    events: Mutex<mpsc::Receiver<Event>>,
    runtime: Mutex<Option<tokio::runtime::Runtime>>,
}

impl Central {
    pub fn start() -> std::io::Result<Central> {
        let runtime = tokio::runtime::Builder::new_multi_thread().worker_threads(2).enable_all().build()?;
        let (commands, command_rx) = unbounded_channel();
        let (event_tx, events) = mpsc::channel();
        runtime.spawn(Actor::new(event_tx).run(command_rx));
        Ok(Central { commands, events: Mutex::new(events), runtime: Mutex::new(Some(runtime)) })
    }

    pub fn send(&self, command: Command) {
        let _ = self.commands.send(command);
    }

    /// The next event, or None after `timeout`. One poller at a time.
    pub fn poll(&self, timeout: Duration) -> Option<Event> {
        self.events.lock().ok()?.recv_timeout(timeout).ok()
    }

    /// Stops the runtime without waiting for a radio call that is still in flight.
    pub fn shutdown(&self) {
        if let Some(runtime) = self.runtime.lock().ok().and_then(|mut r| r.take()) {
            runtime.shutdown_background();
        }
    }
}

type Link = Arc<Mutex<LinkState>>;

#[derive(Default)]
struct LinkState {
    peripheral: Option<Peripheral>,
    id: Option<String>,
    characteristics: HashMap<Uuid, Characteristic>,
    notifications: Option<JoinHandle<()>>,
}

enum Gatt {
    Discover { service: Uuid, wanted: Vec<Uuid> },
    Subscribe(Uuid),
    Read(Uuid),
    Write(Uuid, Vec<u8>),
}

struct Actor {
    events: mpsc::Sender<Event>,
    link: Link,
    adapter: Option<Adapter>,
    last_state: Option<AdapterCode>,
    scanning_for: Option<Uuid>,
    announced: HashSet<String>,
    gatt: Option<UnboundedSender<Gatt>>,
}

impl Actor {
    fn new(events: mpsc::Sender<Event>) -> Actor {
        Actor {
            events,
            link: Arc::new(Mutex::new(LinkState::default())),
            adapter: None,
            last_state: None,
            scanning_for: None,
            announced: HashSet::new(),
            gatt: None,
        }
    }

    fn emit(&self, event: Event) {
        let _ = self.events.send(event);
    }

    fn set_state(&mut self, state: AdapterCode, force: bool) {
        if force || self.last_state != Some(state) {
            self.last_state = Some(state);
            self.emit(Event::AdapterState(state));
        }
    }

    async fn run(mut self, mut commands: UnboundedReceiver<Command>) {
        let (gatt_tx, gatt_rx) = unbounded_channel();
        self.gatt = Some(gatt_tx);
        tokio::spawn(gatt_worker(gatt_rx, self.link.clone(), self.events.clone()));
        let mut tick = tokio::time::interval(STATE_POLL);
        // Held here, not in the actor: the stream is Send but not Sync.
        let mut stream: Option<Pin<Box<dyn Stream<Item = CentralEvent> + Send>>> = None;
        loop {
            tokio::select! {
                command = commands.recv() => match command {
                    Some(command) => self.handle(command, &mut stream).await,
                    None => return,
                },
                event = next_event(&mut stream) => match event {
                    Some(event) => self.on_central_event(event, &mut stream).await,
                    None => stream = None,
                },
                _ = tick.tick() => self.poll_adapter(false, &mut stream).await,
            }
        }
    }

    /// Opens the first adapter if there is none yet, and reports its power state.
    async fn poll_adapter(&mut self, force: bool, stream: &mut Option<Pin<Box<dyn Stream<Item = CentralEvent> + Send>>>) {
        if self.adapter.is_none() {
            self.adapter = open_adapter().await;
            if let Some(adapter) = &self.adapter {
                *stream = adapter.events().await.ok();
            }
        }
        let state = match &self.adapter {
            // A machine with no usable adapter is the same to the wearer as Bluetooth being off.
            None => AdapterCode::Off,
            Some(adapter) => match adapter.adapter_state().await {
                Ok(CentralState::PoweredOn) => AdapterCode::On,
                Ok(CentralState::PoweredOff) => AdapterCode::Off,
                _ => AdapterCode::Unknown,
            },
        };
        self.set_state(state, force);
    }

    async fn handle(&mut self, command: Command, stream: &mut Option<Pin<Box<dyn Stream<Item = CentralEvent> + Send>>>) {
        match command {
            Command::Refresh => self.poll_adapter(true, stream).await,
            Command::StartScan(service) => {
                self.announced.clear();
                self.scanning_for = Some(service);
                if let Some(adapter) = &self.adapter {
                    if let Err(e) = adapter.start_scan(ScanFilter { services: vec![service] }).await {
                        eprintln!("neurone-btle: start_scan failed: {e}");
                    }
                }
            }
            Command::StopScan => {
                self.scanning_for = None;
                if let Some(adapter) = &self.adapter {
                    let _ = adapter.stop_scan().await;
                }
            }
            Command::Connect(id) => self.connect(id),
            Command::Disconnect => {
                let peripheral = self.link.lock().unwrap().peripheral.clone();
                if let Some(p) = peripheral {
                    tokio::spawn(async move {
                        let _ = p.disconnect().await;
                    });
                }
            }
            Command::Discover { service, wanted } => self.queue(Gatt::Discover { service, wanted }),
            Command::EnableNotifications(u) => self.queue(Gatt::Subscribe(u)),
            Command::Read(u) => self.queue(Gatt::Read(u)),
            Command::Write(u, v) => self.queue(Gatt::Write(u, v)),
        }
    }

    fn queue(&self, op: Gatt) {
        if let Some(tx) = &self.gatt {
            let _ = tx.send(op);
        }
    }

    fn connect(&mut self, id: String) {
        let Some(adapter) = self.adapter.clone() else {
            self.emit(Event::Disconnected(id));
            return;
        };
        let link = self.link.clone();
        let events = self.events.clone();
        tokio::spawn(async move {
            let peripheral = match adapter.peripherals().await {
                Ok(list) => list.into_iter().find(|p| p.id().to_string() == id),
                Err(_) => None,
            };
            let Some(peripheral) = peripheral else {
                let _ = events.send(Event::Disconnected(id));
                return;
            };
            {
                let mut state = link.lock().unwrap();
                state.peripheral = Some(peripheral.clone());
                state.id = Some(id.clone());
            }
            match peripheral.connect_with_timeout(CONNECT_TIMEOUT).await {
                Ok(()) => {
                    if let Ok(stream) = peripheral.notifications().await {
                        let events = events.clone();
                        let task = tokio::spawn(async move {
                            let mut stream = stream;
                            while let Some(n) = stream.next().await {
                                let _ = events.send(Event::CharacteristicChanged(n.uuid, n.value));
                            }
                        });
                        link.lock().unwrap().notifications = Some(task);
                    }
                    let _ = events.send(Event::Connected(id));
                }
                Err(e) => {
                    eprintln!("neurone-btle: connect failed: {e}");
                    clear_link(&link);
                    // A failed connect is a disconnect to the manager: it rescans after its delay.
                    let _ = events.send(Event::Disconnected(id));
                }
            }
        });
    }

    async fn on_central_event(&mut self, event: CentralEvent, stream: &mut Option<Pin<Box<dyn Stream<Item = CentralEvent> + Send>>>) {
        match event {
            CentralEvent::StateUpdate(_) => self.poll_adapter(false, stream).await,
            CentralEvent::DeviceDiscovered(id) | CentralEvent::DeviceUpdated(id) => {
                let Some(service) = self.scanning_for else { return };
                let key = id.to_string();
                if self.announced.contains(&key) {
                    return;
                }
                // BlueZ merges scan filters across every program on the machine, so on Linux the
                // service is checked again here; elsewhere the platform's filter is trusted.
                if cfg!(target_os = "linux") && !self.advertises(&id, service).await {
                    return;
                }
                self.announced.insert(key.clone());
                self.emit(Event::DeviceFound(key));
            }
            CentralEvent::DeviceDisconnected(id) => {
                let key = id.to_string();
                let ours = self.link.lock().unwrap().id.as_deref() == Some(key.as_str());
                if ours {
                    clear_link(&self.link);
                    self.emit(Event::Disconnected(key));
                }
            }
            _ => {}
        }
    }

    async fn advertises(&self, id: &btleplug::platform::PeripheralId, service: Uuid) -> bool {
        let Some(adapter) = &self.adapter else { return false };
        let Ok(p) = adapter.peripheral(id).await else { return false };
        match p.properties().await {
            Ok(Some(props)) => props.services.contains(&service),
            _ => false,
        }
    }
}

fn clear_link(link: &Link) {
    let mut state = link.lock().unwrap();
    if let Some(task) = state.notifications.take() {
        task.abort();
    }
    state.peripheral = None;
    state.id = None;
    state.characteristics.clear();
}

async fn next_event(
    stream: &mut Option<Pin<Box<dyn Stream<Item = CentralEvent> + Send>>>,
) -> Option<CentralEvent> {
    match stream {
        Some(s) => s.next().await,
        None => std::future::pending().await,
    }
}

async fn open_adapter() -> Option<Adapter> {
    let manager = Manager::new().await.ok()?;
    manager.adapters().await.ok()?.into_iter().next()
}

/// Runs GATT operations one at a time, in the order they were queued.
async fn gatt_worker(mut ops: UnboundedReceiver<Gatt>, link: Link, events: mpsc::Sender<Event>) {
    while let Some(op) = ops.recv().await {
        let Some(peripheral) = link.lock().unwrap().peripheral.clone() else { continue };
        match op {
            Gatt::Discover { service, wanted } => {
                if let Err(e) = peripheral.discover_services().await {
                    eprintln!("neurone-btle: discover failed: {e}");
                    continue;
                }
                let mut found = Vec::new();
                {
                    let mut state = link.lock().unwrap();
                    state.characteristics.clear();
                    for c in peripheral.characteristics() {
                        if c.service_uuid == service && wanted.contains(&c.uuid) {
                            found.push(c.uuid);
                            state.characteristics.insert(c.uuid, c);
                        }
                    }
                }
                let _ = events.send(Event::CharacteristicsDiscovered(found));
            }
            Gatt::Subscribe(u) => {
                if let Some(c) = characteristic(&link, u) {
                    if let Err(e) = peripheral.subscribe(&c).await {
                        eprintln!("neurone-btle: subscribe {u} failed: {e}");
                    }
                }
            }
            Gatt::Read(u) => {
                if let Some(c) = characteristic(&link, u) {
                    match peripheral.read(&c).await {
                        Ok(v) => {
                            let _ = events.send(Event::CharacteristicRead(u, v));
                        }
                        // An encrypted characteristic read on an unpaired link lands here.
                        Err(e) => {
                            eprintln!("neurone-btle: read {u} failed: {e}");
                            let _ = events.send(Event::CharacteristicReadFailed(u));
                        }
                    }
                }
            }
            Gatt::Write(u, v) => {
                if let Some(c) = characteristic(&link, u) {
                    if let Err(e) = peripheral.write(&c, &v, WriteType::WithResponse).await {
                        eprintln!("neurone-btle: write {u} failed: {e}");
                    }
                }
            }
        }
    }
}

fn characteristic(link: &Link, uuid: Uuid) -> Option<Characteristic> {
    link.lock().unwrap().characteristics.get(&uuid).cloned()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn events_encode_to_the_documented_wire_form() {
        let u = Uuid::parse_str("00000001-0000-1000-8000-00805f9b34fb").unwrap();
        assert_eq!(Event::AdapterState(AdapterCode::On).encode(), vec![1, 3]);
        assert_eq!(Event::DeviceFound("ab".into()).encode(), vec![2, b'a', b'b']);
        assert_eq!(Event::Connected("x".into()).encode(), vec![3, b'x']);
        assert_eq!(Event::Disconnected("x".into()).encode(), vec![4, b'x']);
        let mut two = vec![5];
        two.extend_from_slice(u.as_bytes());
        two.extend_from_slice(u.as_bytes());
        assert_eq!(Event::CharacteristicsDiscovered(vec![u, u]).encode(), two);
        let mut changed = vec![6];
        changed.extend_from_slice(u.as_bytes());
        changed.extend_from_slice(&[9, 8]);
        assert_eq!(Event::CharacteristicChanged(u, vec![9, 8]).encode(), changed);
        changed[0] = 7;
        assert_eq!(Event::CharacteristicRead(u, vec![9, 8]).encode(), changed);
        let mut failed = vec![8];
        failed.extend_from_slice(u.as_bytes());
        assert_eq!(Event::CharacteristicReadFailed(u).encode(), failed);
    }

    #[test]
    fn adapter_ordinals_match_the_kotlin_enum() {
        // AdapterState { UNKNOWN, OFF, UNAUTHORIZED, ON }
        assert_eq!(
            [AdapterCode::Unknown, AdapterCode::Off, AdapterCode::Unauthorized, AdapterCode::On].map(|s| s as u8),
            [0, 1, 2, 3]
        );
    }

    #[test]
    fn a_central_starts_and_stops_without_a_radio() {
        // Runs on a machine with or without Bluetooth: starting must not panic and polling must
        // time out cleanly rather than block.
        let central = Central::start().expect("runtime");
        central.send(Command::Refresh);
        let _ = central.poll(Duration::from_millis(50));
        central.shutdown();
    }
}
