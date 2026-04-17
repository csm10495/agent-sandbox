//! Minimal bash-like command line tokenizer.
//!
//! Supports:
//!   * Whitespace-separated arguments.
//!   * Single-quoted strings (literal, no escapes).
//!   * Double-quoted strings with `\"`, `\\`, `\n`, `\t`, `\r`, `\0` escapes.
//!   * Backslash escapes outside of quotes.
//!   * `#` starts a comment to end-of-line (outside quotes).
//!
//! Returned tokens are the argv for one command. Empty input yields `[]`.
//! An unterminated quote returns [`ParseError::UnterminatedQuote`].

use alloc::string::String;
use alloc::vec::Vec;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ParseError {
    UnterminatedQuote,
    TrailingBackslash,
}

pub fn tokenize(input: &str) -> Result<Vec<String>, ParseError> {
    let mut out: Vec<String> = Vec::new();
    let mut current = String::new();
    let mut in_token = false;
    let mut iter = input.chars().peekable();

    while let Some(c) = iter.next() {
        match c {
            '#' if !in_token => {
                // Comment: discard to end of line.
                while iter.next().is_some() {}
                break;
            }
            c if c.is_whitespace() => {
                if in_token {
                    out.push(core::mem::take(&mut current));
                    in_token = false;
                }
            }
            '\'' => {
                in_token = true;
                loop {
                    match iter.next() {
                        None => return Err(ParseError::UnterminatedQuote),
                        Some('\'') => break,
                        Some(ch) => current.push(ch),
                    }
                }
            }
            '"' => {
                in_token = true;
                loop {
                    match iter.next() {
                        None => return Err(ParseError::UnterminatedQuote),
                        Some('"') => break,
                        Some('\\') => match iter.next() {
                            None => return Err(ParseError::UnterminatedQuote),
                            Some('n') => current.push('\n'),
                            Some('t') => current.push('\t'),
                            Some('r') => current.push('\r'),
                            Some('0') => current.push('\0'),
                            Some('\\') => current.push('\\'),
                            Some('"') => current.push('"'),
                            Some(ch) => {
                                current.push('\\');
                                current.push(ch);
                            }
                        },
                        Some(ch) => current.push(ch),
                    }
                }
            }
            '\\' => match iter.next() {
                None => return Err(ParseError::TrailingBackslash),
                Some(ch) => {
                    in_token = true;
                    current.push(ch);
                }
            },
            c => {
                in_token = true;
                current.push(c);
            }
        }
    }
    if in_token {
        out.push(current);
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    use alloc::string::ToString;

    fn v(parts: &[&str]) -> Vec<String> {
        parts.iter().map(|s| s.to_string()).collect()
    }

    #[test]
    fn empty_and_whitespace() {
        assert_eq!(tokenize("").unwrap(), v(&[]));
        assert_eq!(tokenize("   \t  ").unwrap(), v(&[]));
    }

    #[test]
    fn simple_words() {
        assert_eq!(tokenize("ls -la /tmp").unwrap(), v(&["ls", "-la", "/tmp"]));
    }

    #[test]
    fn single_quoted_preserves_spaces() {
        assert_eq!(
            tokenize("echo 'hello   world'").unwrap(),
            v(&["echo", "hello   world"])
        );
    }

    #[test]
    fn double_quoted_with_escapes() {
        assert_eq!(
            tokenize(r#"echo "a\nb\tc\"d""#).unwrap(),
            v(&["echo", "a\nb\tc\"d"])
        );
    }

    #[test]
    fn backslash_escape_outside_quotes() {
        assert_eq!(tokenize(r"echo a\ b").unwrap(), v(&["echo", "a b"]));
    }

    #[test]
    fn unterminated_single_quote() {
        assert_eq!(tokenize("echo 'abc"), Err(ParseError::UnterminatedQuote));
    }

    #[test]
    fn unterminated_double_quote() {
        assert_eq!(tokenize(r#"echo "abc"#), Err(ParseError::UnterminatedQuote));
    }

    #[test]
    fn trailing_backslash() {
        assert_eq!(tokenize("echo \\"), Err(ParseError::TrailingBackslash));
    }

    #[test]
    fn comments() {
        assert_eq!(tokenize("echo hi # comment").unwrap(), v(&["echo", "hi"]));
        assert_eq!(tokenize("# full comment").unwrap(), v(&[]));
    }

    #[test]
    fn mixed_quoting_concatenation() {
        assert_eq!(tokenize(r#"a'b'"c"d"#).unwrap(), v(&["abcd"]));
    }
}
