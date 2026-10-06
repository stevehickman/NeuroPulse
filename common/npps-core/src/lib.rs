//! NeurOne NPPS core (OI-NPPS-CORE-01).
//!
//! One lexer, parser and field table for every runtime. The reference behaviour is
//! `common/lib/nppsParser.ts`; `tests/differential.rs` diffs this crate's output against
//! what that parser produces for the whole shipped library and the shared fixtures.
//!
//! v0 covers `protocol` entries and the descriptor compiler. `composite`, `limits`, `zone`, `condition` and
//! `wavelength_rules` blocks are skipped (reported, not validated); see the open item.

pub mod compiler;
pub mod error;
pub mod js;
pub mod lexer;
pub mod parser;
pub mod table;
pub mod wavelength;

pub use error::ParseError;
pub use compiler::{compile_protocol, CompileOptions, Compiled};
pub use parser::{parse_npps, Entry};
