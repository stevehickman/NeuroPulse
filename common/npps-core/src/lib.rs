//! NeurOne NPPS core (OI-NPPS-CORE-01).
//!
//! One lexer, parser and field table for every runtime. The reference behaviour is
//! `common/lib/nppsParser.ts`; `tests/differential.rs` diffs this crate's output against
//! what that parser produces for the whole shipped library and the shared fixtures.
//!
//! Covers every block of an `.npps` file (`protocol`, `composite`, `limits`, `zone`, `condition`,
//! `wavelength_rules`) and the descriptor compiler.

pub mod api;
pub mod compiler;
pub mod error;
pub mod js;
pub mod lexer;
pub mod parser;
pub mod table;
pub mod wavelength;

pub use error::ParseError;
pub use compiler::{compile_protocol, CompileOptions, Compiled};
pub use parser::{parse_file, parse_npps, Entry, ParsedFile};
