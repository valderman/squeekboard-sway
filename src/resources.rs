/*! Statically linked resources.
 * This could be done using GResource, but that would need additional work.
 */

use std::collections::HashMap;

use std::iter::FromIterator;

// TODO: keep a list of what is a language layout,
// and what a convenience layout. "_wide" is not a layout,
// neither is "number"
const KEYBOARDS: &[(*const str, *const str)] = &[
    ("us", include_str!("../data/keyboards/us.yaml")),
    ("us_wide", include_str!("../data/keyboards/us_wide.yaml")),
    ("de", include_str!("../data/keyboards/de.yaml")),
    ("el", include_str!("../data/keyboards/el.yaml")),
    ("es", include_str!("../data/keyboards/es.yaml")),
    ("it", include_str!("../data/keyboards/it.yaml")),
    ("ja+kana", include_str!("../data/keyboards/ja+kana.yaml")),
    ("no", include_str!("../data/keyboards/no.yaml")),
    ("number", include_str!("../data/keyboards/number.yaml")),
    ("se", include_str!("../data/keyboards/se.yaml")),
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

/// Translations of the layout identifier strings
const LAYOUT_NAMES: &[(*const str, *const str)] = &[
    ("en-US", include_str!("../data/langs/en-US.txt")),
    ("pl-PL", include_str!("../data/langs/pl-PL.txt")),
];

pub fn get_layout_names(lang: &str)
    -> Option<HashMap<&'static str, &'static str>>
{
    let translations = LAYOUT_NAMES.iter()
        .find(|(name, _data)| {
            let name: *const str = *name;
            (unsafe { &*name }) == lang
        })
        .map(|(_name, data)| {
            let data: *const str = *data;
            unsafe { &*data }
        });
    translations.map(make_mapping)
}

fn parse_line(line: &str) -> Option<(&str, &str)> {
    let comment = line.trim().starts_with("#");
    if comment {
        None
    } else {
        let mut iter = line.splitn(2, " ");
        let name = iter.next().unwrap();
        // will skip empty and unfinished lines
        iter.next().map(|tr| (name, tr.trim()))
    }
}

fn make_mapping(data: &str) -> HashMap<&str, &str> {
    HashMap::from_iter(
        data.split("\n")
            .filter_map(parse_line)
    )
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn mapping_line() {
        assert_eq!(
            Some(("name", "translation")),
            parse_line("name translation")
        );
    }

    #[test]
    fn mapping_bad() {
        assert_eq!(None, parse_line("bad"));
    }

    #[test]
    fn mapping_empty() {
        assert_eq!(None, parse_line(""));
    }

    #[test]
    fn mapping_comment() {
        assert_eq!(None, parse_line("# comment"));
    }

    #[test]
    fn mapping_comment_offset() {
        assert_eq!(None, parse_line("  # comment"));
    }
}
