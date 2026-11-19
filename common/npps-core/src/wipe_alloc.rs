//! An allocator that overwrites every block with zeros before it releases it (`SWR-MG-28` (g); `OI-PRIV-12`, option C).
//!
//! `Zeroizing` clears the buffers the core knows hold a secret. It cannot clear the copies nobody named: the parsed form
//! of a request (the JSON parser copies every text field into a string of its own, a key in hex and a passphrase among
//! them), a buffer that grew and left its shorter self behind, an intermediate result. They are released as they are, and
//! in WebAssembly, whose linear memory is never returned to the system, they stay readable for the life of the page.
//! Measured in the browser build before this existed: one call of `passphraseSecret` left a copy of the passphrase in
//! memory, and one `sealValue` left the hex key and the plaintext value.
//!
//! So the binding crates (`npps-ffi`, `npps-jni`) install this as their global allocator, and then **every** block the
//! Rust side releases is cleared, whatever held it and whatever the operation. Every block, not only one known to hold a
//! secret: the allocator cannot tell which is which, and a wipe that depended on the content would say which calls
//! carried one (`CLAUDE.md` §5.1, rule 2).
//!
//! What it does not reach: the host's own copies (a JVM, JavaScript, Swift or C# string), the stack, and memory a
//! dependency obtains some other way than the Rust allocator. The core itself installs nothing: a library does not choose
//! its user's allocator, and the core's own tests run without it.

use core::alloc::{GlobalAlloc, Layout};
use core::sync::atomic::{AtomicUsize, Ordering};
use zeroize::Zeroize;

/// `A`, with every block overwritten with zeros before `A` takes it back.
///
/// `realloc` is deliberately the trait's default: allocate the new block, copy, release the old one **through
/// [`dealloc`](GlobalAlloc::dealloc)**, so a buffer that grows or shrinks leaves zeros where it was. An in-place resize
/// would be faster and would leave the tail of a shrunk block, or a moved block, as it was.
pub struct Wiping<A>(pub A);

/// A function that is shown each block (its address and size) after it has been wiped and before it is released, or 0.
static OBSERVER: AtomicUsize = AtomicUsize::new(0);

/// For tests of what is released: `observer` is shown every block after the wipe, while it is still valid, so a test can
/// check it is zeros without reading memory that has been released. `None` turns it off. The observer must not allocate.
/// Nothing but tests calls this; it is not part of the C ABI.
pub fn observe_releases(observer: Option<fn(*const u8, usize)>) {
    OBSERVER.store(observer.map_or(0, |f| f as usize), Ordering::SeqCst);
}

unsafe impl<A: GlobalAlloc> GlobalAlloc for Wiping<A> {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        self.0.alloc(layout)
    }

    unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 {
        self.0.alloc_zeroed(layout)
    }

    unsafe fn dealloc(&self, block: *mut u8, layout: Layout) {
        // `zeroize` writes with volatile semantics, so the compiler cannot drop the store as dead before a release.
        core::slice::from_raw_parts_mut(block, layout.size()).zeroize();
        let observer = OBSERVER.load(Ordering::Relaxed);
        if observer != 0 {
            // SAFETY: the only writer stores a `fn(*const u8, usize)` or 0.
            let observer: fn(*const u8, usize) = core::mem::transmute(observer);
            observer(block, layout.size());
        }
        self.0.dealloc(block, layout)
    }
}
