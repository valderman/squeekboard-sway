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
 *   Logs being outputs, they get returned
 *   instead of being misleadingly passed back through arguments.
 *   It seems more difficult to pass the logger around,
 *   but this may be a solved problem from the area of functional programming.
 * 
 * This library generally aims at the approach in 3.
 * */

use std::fmt::Display;

/// Levels are not in order.
pub enum Level {
    // Levels for reporting violated constraints
    /// The program violated a self-imposed constraint,
    /// ended up in an inconsistent state, and cannot recover.
    /// Handlers must not actually panic. (should they?)
    Panic,
    /// The program violated a self-imposed constraint,
    /// ended up in an inconsistent state, but some state can be recovered.
    Bug,
    /// Invalid data given by an external source,
    /// some state of the program must be dropped.
    Error,
    // Still violated constraints, but harmless
    /// Invalid data given by an external source, parts of data are ignored.
    /// No previous program state needs to be dropped.
    Warning,
    /// External source not in an expected state,
    /// but not violating any protocols (including no relevant protocol).
    Surprise,
    // Informational
    /// A change in internal state that results in a change of behaviour
    /// that a user can observe, and a tinkerer might find useful.
    /// E.g. selection of external sources, like loading user's UI files,
    /// language switch, overrides.
    Info,
    /// Information useful for application developer only.
    /// Should be limited to information gotten from external sources,
    /// and more tricky parts of internal state.
    Debug,
}

impl Level {
    fn as_str(&self) -> &'static str {
        match self {
            Level::Panic => "Panic",
            Level::Bug => "Bug",
            Level::Error => "Error",
            Level::Warning => "Warning",
            Level::Surprise => "Surprise",
            Level::Info => "Info",
            Level::Debug => "Debug",
        }
    }
}

impl From<Problem> for Level {
    fn from(problem: Problem) -> Level {
        use self::Level::*;
        match problem {
            Problem::Panic => Panic,
            Problem::Bug => Bug,
            Problem::Error => Error,
            Problem::Warning => Warning,
            Problem::Surprise => Surprise,
        }
    }
}

/// Only levels which indicate problems
/// To use with `Result::Err` handlers,
/// which are needed only when something went off the optimal path.
/// A separate type ensures that `Err`
/// can't end up misclassified as a benign event like `Info`.
pub enum Problem {
    Panic,
    Bug,
    Error,
    Warning,
    Surprise,
}

/// Sugar for approach 2
// TODO: avoid, deprecate.
// Handler instances should be long lived, not one per call.
macro_rules! log_print {
    ($level:expr, $($arg:tt)*) => (::logging::print($level, &format!($($arg)*)))
}

/// Approach 2
pub fn print(level: Level, message: &str) {
    Print{}.handle(level, message)
}

/// Sugar for logging errors in results.
pub trait Warn where Self: Sized {
    type Value;
    /// Approach 2.
    fn or_print(self, level: Problem, message: &str) -> Option<Self::Value> {
        self.or_warn(&mut Print {}, level.into(), message)
    }
    /// Approach 3.
    fn or_warn<H: Handler>(
        self,
        handler: &mut H,
        level: Problem,
        message: &str,
    ) -> Option<Self::Value>;
}

impl<T, E: Display> Warn for Result<T, E> {
    type Value = T;
    fn or_warn<H: Handler>(
        self,
        handler: &mut H,
        level: Problem,
        message: &str,
    ) -> Option<T> {
        self.map_err(|e| {
            handler.handle(level.into(), &format!("{}: {}", message, e));
            e
        }).ok()
    }
}

impl<T> Warn for Option<T> {
    type Value = T;
    fn or_warn<H: Handler>(
        self,
        handler: &mut H,
        level: Problem,
        message: &str,
    ) -> Option<T> {
        self.or_else(|| {
            handler.handle(level.into(), message);
            None
        })
    }
}

/// A mutable handler for text warnings.
/// Approach 3.
pub trait Handler {
    /// Handle a log message
    fn handle(&mut self, level: Level, message: &str);
}

/// Prints info to stdout, everything else to stderr
pub struct Print;

impl Handler for Print {
    fn handle(&mut self, level: Level, message: &str) {
        match level {
            Level::Info => println!("Info: {}", message),
            l => eprintln!("{}: {}", l.as_str(), message),
        }
    }
}

/// Warning handler that will panic
/// at any warning, error, surprise, bug, or panic.
/// Don't use except in tests
pub struct ProblemPanic;

impl Handler for ProblemPanic {
    fn handle(&mut self, level: Level, message: &str) {
        use self::Level::*;
        match level {
            Panic | Bug | Error | Warning | Surprise => panic!("{}", message),
            l => Print{}.handle(l, message),
        }
    }
}
