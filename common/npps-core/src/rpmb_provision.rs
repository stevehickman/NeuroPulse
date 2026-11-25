//! The eMMC RPMB authentication-key programming step, as a host-checkable model (`REQ-RPMBK-02` to
//! `REQ-RPMBK-04`, `NP-REG-UPG-001` §7.10, `OI-PRIV-32` (NP-PRIV-ANALYSIS-004), GitHub #960).
//!
//! WHAT THIS IS. The decision logic of the factory step that programs a helmet's RPMB key into its eMMC: look
//! at the part first, program the key only into a part that says it has none, never send a second
//! key-programming request, prove the result with an authenticated read whose MAC verifies under the key and
//! whose nonce comes back, and refuse a part that already holds some other key. It runs against any
//! [`RpmbPart`]; its tests (`tests/rpmb_provision.rs`) drive it against [`SimRpmb`], an in-memory part with the behaviour this step relies
//! on. Frames are built and checked byte for byte, MAC included, so a layout error fails a test.
//!
//! WHAT THIS IS NOT. It chooses no key. Where the key comes from is undecided
//! (`docs/human-input/oi-priv-32-rpmb-key-source.md`), so the key is an INPUT. It writes no floor: the floor's
//! block layout belongs to the RPMB floor store, which is built separately. It is not the device driver: a
//! real exchange is CMD23/CMD25 to send a frame and CMD18 to read the response (JESD84), and [`RpmbPart`]
//! stands for that whole round trip. No production key may be placed in this crate, and no default or
//! constant key exists anywhere in it.
//!
//! FRAME LAYOUT. The 512-byte RPMB data frame, as OP-TEE lays it out (`optee_os` `core/tee/tee_rpmb_fs.c`,
//! `struct rpmb_data_frame`, read 2026-11-23): 196 stuff bytes, 32-byte key/MAC, 256 data bytes, 16-byte nonce,
//! 4-byte write counter, 2-byte address, 2-byte block count, 2-byte result, 2-byte request/response type, every
//! multi-byte field most significant byte first. The MAC is HMAC-SHA256 over bytes 228 to 511. The message
//! types and result codes are OP-TEE's `RPMB_MSG_TYPE_*` and `RPMB_RESULT_*`. JESD84 itself was not read
//! (JEDEC requires an account), so this is checked against one widely used implementation, not the standard.

use hmac::{Hmac, Mac};
use sha2::Sha256;
use zeroize::Zeroize;

/// Bytes in one RPMB data frame.
pub const FRAME_LEN: usize = 512;
/// Bytes in the authentication key, and in the MAC.
pub const KEY_LEN: usize = 32;
/// Bytes in the nonce.
pub const NONCE_LEN: usize = 16;

pub const OFF_KEY_MAC: usize = 196;
pub const OFF_DATA: usize = 228;
pub const OFF_NONCE: usize = 484;
pub const OFF_WRITE_COUNTER: usize = 500;
pub const OFF_RESULT: usize = 508;
pub const OFF_TYPE: usize = 510;
/// The MAC covers the data field to the end of the frame.
const MAC_FROM: usize = OFF_DATA;

/// Request: program the authentication key.
pub const REQ_KEY_PROGRAM: u16 = 0x0001;
/// Request: read the write counter (the response carries a MAC once a key is programmed).
pub const REQ_WRITE_COUNTER_READ: u16 = 0x0002;
/// Request: read the result of the last write.
pub const REQ_RESULT_READ: u16 = 0x0005;
/// Response to [`REQ_KEY_PROGRAM`].
pub const RESP_KEY_PROGRAM: u16 = 0x0100;
/// Response to [`REQ_WRITE_COUNTER_READ`].
pub const RESP_WRITE_COUNTER_READ: u16 = 0x0200;

/// `RPMB_RESULT_OK`.
pub const RESULT_OK: u16 = 0x00;
/// `RPMB_RESULT_GENERAL_FAILURE`.
pub const RESULT_GENERAL_FAILURE: u16 = 0x01;
/// `RPMB_RESULT_AUTH_KEY_NOT_PROGRAMMED`.
pub const RESULT_KEY_NOT_PROGRAMMED: u16 = 0x07;
/// The result code proper (`RPMB_RESULT_MASK`).
const RESULT_MASK: u16 = 0x3F;
/// The write counter has expired, so no further authenticated write will ever succeed (`RPMB_RESULT_WR_CNT_EXPIRED`).
pub const RESULT_WRITE_COUNTER_EXPIRED: u16 = 0x80;

pub type Frame = [u8; FRAME_LEN];

/// One request-and-response round trip with the part's RPMB. A transport failure is an error, never a frame.
pub trait RpmbPart {
    fn exchange(&mut self, request: &Frame) -> Result<Frame, TransportError>;
}

/// The bus or the driver failed; nothing is known about the part.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TransportError;

/// What the step found and did, when it accepts the part.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Accepted {
    /// The part had no key. This run programmed it, then proved it with an authenticated read.
    Programmed { write_counter: u32 },
    /// The part already held this unit's key (a run interrupted after programming, re-run). Nothing was
    /// programmed; the authenticated read proved the key.
    AlreadyThisKey { write_counter: u32 },
}

/// Why the step refused. Every refusal ends the unit's run: Quality dispositions it (§7.10 "When a unit is refused").
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Refusal {
    /// The key is all zero or all one bits: an unprogrammed source, a stub or an erased fuse. Nothing was sent.
    DegenerateKey,
    /// The nonce source returned the same nonce twice in one run, or an all-zero nonce. Nothing more was sent.
    BadNonce,
    /// The bus failed.
    Transport,
    /// The part already holds a key, and it is not this unit's. It is never re-keyed (JESD84 allows one
    /// programming), so it cannot be used in this unit. Quarantine.
    OtherKey,
    /// The response's type is not the one the request asks for.
    WrongResponseType { got: u16 },
    /// The response's nonce is not the one sent: a replayed or forged answer.
    NonceMismatch,
    /// The write counter has expired: no floor could ever be raised on this part.
    CounterExpired,
    /// The part reported an error code other than "key not programmed".
    PartError { code: u16 },
    /// The key-programming request was answered with an error.
    ProgramFailed { code: u16 },
    /// The part accepted the key, then still reported no key at the proving read.
    ProgramNotTaken,
}

/// What the first read found.
enum Found {
    Blank,
    Keyed { write_counter: u32 },
}

/// The step. `key` is this unit's RPMB key, from whatever source is decided. `next_nonce` supplies a fresh
/// random nonce per read (the device's TRNG in a real run).
///
/// Order: (1) refuse a degenerate key before anything is sent; (2) authenticated write-counter read; (3) only if
/// the part says "key not programmed", one key-programming request; (4) a second authenticated read with a new
/// nonce, which must verify under the key. There is one call site that sends the key, and it is reached only
/// from a part that reported no key in this run.
pub fn provision(
    part: &mut dyn RpmbPart,
    key: &[u8; KEY_LEN],
    next_nonce: &mut dyn FnMut() -> [u8; NONCE_LEN],
) -> Result<Accepted, Refusal> {
    if is_degenerate(key) {
        return Err(Refusal::DegenerateKey);
    }
    let mut used: Option<[u8; NONCE_LEN]> = None;
    let mut fresh_nonce = |used: &mut Option<[u8; NONCE_LEN]>| -> Result<[u8; NONCE_LEN], Refusal> {
        let n = next_nonce();
        if n == [0u8; NONCE_LEN] || used.as_ref() == Some(&n) {
            return Err(Refusal::BadNonce);
        }
        *used = Some(n);
        Ok(n)
    };

    let first = fresh_nonce(&mut used)?;
    match read_counter(part, key, &first)? {
        Found::Keyed { write_counter } => Ok(Accepted::AlreadyThisKey { write_counter }),
        Found::Blank => {
            program_key(part, key)?;
            let second = fresh_nonce(&mut used)?;
            match read_counter(part, key, &second)? {
                Found::Keyed { write_counter } => Ok(Accepted::Programmed { write_counter }),
                Found::Blank => Err(Refusal::ProgramNotTaken),
            }
        }
    }
}

/// All zero or all one bits: what an unprogrammed fuse field, an erased store or a stub returns.
pub fn is_degenerate(key: &[u8; KEY_LEN]) -> bool {
    key.iter().all(|&b| b == 0x00) || key.iter().all(|&b| b == 0xFF)
}

fn read_counter(part: &mut dyn RpmbPart, key: &[u8; KEY_LEN], nonce: &[u8; NONCE_LEN]) -> Result<Found, Refusal> {
    let mut req = [0u8; FRAME_LEN];
    req[OFF_NONCE..OFF_NONCE + NONCE_LEN].copy_from_slice(nonce);
    put_u16(&mut req, OFF_TYPE, REQ_WRITE_COUNTER_READ);
    let resp = part.exchange(&req).map_err(|_| Refusal::Transport)?;

    let ty = get_u16(&resp, OFF_TYPE);
    if ty != RESP_WRITE_COUNTER_READ {
        return Err(Refusal::WrongResponseType { got: ty });
    }
    let result = get_u16(&resp, OFF_RESULT);
    let code = result & RESULT_MASK;
    if code == RESULT_KEY_NOT_PROGRAMMED {
        // An unauthenticated answer. Believing it costs nothing: the only thing it unlocks is one programming
        // request, and a part that lied about having no key refuses that request (JESD84 programs once) or
        // fails the proving read that follows.
        return Ok(Found::Blank);
    }
    // A keyed answer must prove itself before any field of it is believed, the result code included.
    if !mac_verifies(key, &resp) {
        return Err(Refusal::OtherKey);
    }
    if resp[OFF_NONCE..OFF_NONCE + NONCE_LEN] != nonce[..] {
        return Err(Refusal::NonceMismatch);
    }
    if result & RESULT_WRITE_COUNTER_EXPIRED != 0 {
        return Err(Refusal::CounterExpired);
    }
    if code != RESULT_OK {
        return Err(Refusal::PartError { code });
    }
    Ok(Found::Keyed { write_counter: get_u32(&resp, OFF_WRITE_COUNTER) })
}

fn program_key(part: &mut dyn RpmbPart, key: &[u8; KEY_LEN]) -> Result<(), Refusal> {
    let mut req = [0u8; FRAME_LEN];
    req[OFF_KEY_MAC..OFF_KEY_MAC + KEY_LEN].copy_from_slice(key);
    put_u16(&mut req, OFF_TYPE, REQ_KEY_PROGRAM);
    let sent = part.exchange(&req);
    req.zeroize();
    let resp = sent.map_err(|_| Refusal::Transport)?;
    let ty = get_u16(&resp, OFF_TYPE);
    if ty != RESP_KEY_PROGRAM {
        return Err(Refusal::WrongResponseType { got: ty });
    }
    let code = get_u16(&resp, OFF_RESULT) & RESULT_MASK;
    if code != RESULT_OK {
        return Err(Refusal::ProgramFailed { code });
    }
    Ok(())
}

/// HMAC-SHA256 over bytes 228..512 of `frame`.
pub fn frame_mac(key: &[u8; KEY_LEN], frame: &Frame) -> [u8; KEY_LEN] {
    let mut m = <Hmac<Sha256> as Mac>::new_from_slice(key).expect("HMAC takes any key length");
    m.update(&frame[MAC_FROM..]);
    m.finalize().into_bytes().into()
}

fn mac_verifies(key: &[u8; KEY_LEN], frame: &Frame) -> bool {
    let mut m = <Hmac<Sha256> as Mac>::new_from_slice(key).expect("HMAC takes any key length");
    m.update(&frame[MAC_FROM..]);
    m.verify_slice(&frame[OFF_KEY_MAC..OFF_KEY_MAC + KEY_LEN]).is_ok()
}

fn put_u16(f: &mut Frame, off: usize, v: u16) {
    f[off..off + 2].copy_from_slice(&v.to_be_bytes());
}
fn get_u16(f: &Frame, off: usize) -> u16 {
    u16::from_be_bytes([f[off], f[off + 1]])
}
fn get_u32(f: &Frame, off: usize) -> u32 {
    u32::from_be_bytes([f[off], f[off + 1], f[off + 2], f[off + 3]])
}

/// An in-memory RPMB with the behaviour this step relies on, and switches for the ways a real part or an
/// interposer could misbehave. Test support; it is public so a fixture simulator can reuse it.
#[derive(Debug, Default)]
pub struct SimRpmb {
    pub key: Option<[u8; KEY_LEN]>,
    pub write_counter: u32,
    pub counter_expired: bool,
    /// Key-programming requests received, accepted or not.
    pub program_requests: usize,
    /// Every exchange received.
    pub exchanges: usize,
    /// Answer the programming request with OK but keep no key.
    pub drop_programming: bool,
    /// Answer every read with this nonce instead of the one sent (a replayed response).
    pub replay_nonce: Option<[u8; NONCE_LEN]>,
    /// Answer reads with this response type instead.
    pub wrong_type: Option<u16>,
    /// Fail the bus on this exchange (1-based).
    pub fail_exchange: Option<usize>,
}

impl SimRpmb {
    pub fn blank() -> Self {
        SimRpmb::default()
    }
    pub fn keyed(key: [u8; KEY_LEN]) -> Self {
        SimRpmb { key: Some(key), ..SimRpmb::default() }
    }
}

impl RpmbPart for SimRpmb {
    fn exchange(&mut self, request: &Frame) -> Result<Frame, TransportError> {
        self.exchanges += 1;
        if self.fail_exchange == Some(self.exchanges) {
            return Err(TransportError);
        }
        let mut resp = [0u8; FRAME_LEN];
        match get_u16(request, OFF_TYPE) {
            REQ_KEY_PROGRAM => {
                self.program_requests += 1;
                put_u16(&mut resp, OFF_TYPE, RESP_KEY_PROGRAM);
                // A part programs its key once. What a real part answers to a second request was not checked
                // against JESD84; this step never sends one, so it does not depend on the answer.
                let code = if self.key.is_some() {
                    RESULT_GENERAL_FAILURE
                } else {
                    if !self.drop_programming {
                        let mut k = [0u8; KEY_LEN];
                        k.copy_from_slice(&request[OFF_KEY_MAC..OFF_KEY_MAC + KEY_LEN]);
                        self.key = Some(k);
                    }
                    RESULT_OK
                };
                put_u16(&mut resp, OFF_RESULT, code);
            }
            REQ_WRITE_COUNTER_READ => {
                put_u16(&mut resp, OFF_TYPE, self.wrong_type.unwrap_or(RESP_WRITE_COUNTER_READ));
                let nonce = self.replay_nonce.unwrap_or_else(|| {
                    let mut n = [0u8; NONCE_LEN];
                    n.copy_from_slice(&request[OFF_NONCE..OFF_NONCE + NONCE_LEN]);
                    n
                });
                resp[OFF_NONCE..OFF_NONCE + NONCE_LEN].copy_from_slice(&nonce);
                match self.key {
                    None => put_u16(&mut resp, OFF_RESULT, RESULT_KEY_NOT_PROGRAMMED),
                    Some(k) => {
                        resp[OFF_WRITE_COUNTER..OFF_WRITE_COUNTER + 4].copy_from_slice(&self.write_counter.to_be_bytes());
                        let r = if self.counter_expired { RESULT_WRITE_COUNTER_EXPIRED } else { RESULT_OK };
                        put_u16(&mut resp, OFF_RESULT, r);
                        let mac = frame_mac(&k, &resp);
                        resp[OFF_KEY_MAC..OFF_KEY_MAC + KEY_LEN].copy_from_slice(&mac);
                    }
                }
            }
            _ => {
                put_u16(&mut resp, OFF_RESULT, RESULT_GENERAL_FAILURE);
            }
        }
        Ok(resp)
    }
}
