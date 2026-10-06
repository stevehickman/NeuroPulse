use std::fmt;

/// A refusal. `Display` matches the reference parser's `NPPSParseError.message`, so a
/// message compares equal across runtimes.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParseError {
    pub message: String,
    pub line: Option<u32>,
}

impl ParseError {
    pub fn at(message: impl Into<String>, line: u32) -> ParseError {
        ParseError { message: message.into(), line: Some(line) }
    }
    pub fn bare(message: impl Into<String>) -> ParseError {
        ParseError { message: message.into(), line: None }
    }
}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.line {
            Some(l) => write!(f, "Line {l}: {}", self.message),
            None => write!(f, "{}", self.message),
        }
    }
}

impl std::error::Error for ParseError {}
