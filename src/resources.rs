/*! Statically linked resources.
 * This could be done using GResource, but that would need additional work.
 */


const KEYBOARDS: &[(*const str, *const str)] = &[
    ("us", include_str!("../data/keyboards/us.yaml")),
    ("es", include_str!("../data/keyboards/es.yaml")),
    ("nb", include_str!("../data/keyboards/nb.yaml")),
    ("number", include_str!("../data/keyboards/number.yaml")),
];

pub fn get_keyboard(needle: &str) -> Option<&'static str> {
    // Need to dereference in unsafe code
    // comparing *const str to &str will compare pointers
    KEYBOARDS.iter()
        .find(|(name, _)| {
            let name: *const str = *name;
            (unsafe { &*name }) == needle
        })
        .map(|(_, value)| {
            let value: *const str = *value;
            unsafe { &*value }
        })
}
