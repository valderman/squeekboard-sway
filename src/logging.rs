/*! Logging library.
 * 
 * This is probably the only part of squeekboard
 * that should be doing any direct printing.
 * 
 * There are several approaches to logging,
 * in the order of increasing flexibility and/or purity:
 * 
 * 1. `println!` directly
 * 
 *   It can't be easily replaced by a different solution
 * 
 * 2. simple `log!` macro
 * 
 *   Replacing the destination at runtime other than globally would be awkward,
 *   so no easy way to suppress errors for things that don't matter,
 *   but formatting is still easy.
 * 
 * 3. logging to a mutable destination type
 * 
 *   Can be easily replaced, but logging `Result` types,
 *   which should be done by calling a method on the result,
 *   can't be formatted directly.
 *   Cannot be parallelized.
 * 
 * 4. logging to an immutable destination type
 * 
 *   Same as above, except it can be parallelized.
 *   It seems more difficult to pass the logger around,
 *   but this may be a solved problem from the area of functional programming.
 * 
 * This library generally aims at the approach in 3.
 * */

use std::error::Error;

/// Sugar for logging errors in results.
/// Approach 2.
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

/// A mutable handler for text warnings.
/// Approach 3.
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
