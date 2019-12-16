/*! Logging library */

use std::error::Error;

/// Sugar for logging errors in results
pub trait Warn {
    type Value;
    fn ok_warn(self, msg: &str) -> Option<Self::Value>;
}

impl<T, E: Error> Warn for Result<T, E> {
    type Value = T;
    fn ok_warn(self, msg: &str) -> Option<T> {
        self.map_err(|e| {
            eprintln!("{}: {}", msg, e);
            e
        }).ok()
    }
}

pub trait WarningHandler {
    /// Handle a warning
    fn handle(&mut self, warning: &str);
}

/// Prints warnings to stderr
pub struct PrintWarnings;

impl WarningHandler for PrintWarnings {
    fn handle(&mut self, warning: &str) {
        eprintln!("{}", warning);
    }
}

/// Warning handler that will panic at any warning.
/// Don't use except in tests
pub struct PanicWarn;

impl WarningHandler for PanicWarn {
    fn handle(&mut self, warning: &str) {
        panic!("{}", warning);
    }
}
