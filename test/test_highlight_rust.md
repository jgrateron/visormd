# Rust Highlight Test

Keywords, primitives, prelude types, lifetimes, raw strings, char literals, comments, and numbers.

```rust
// first_then_push: toma prestado y luego muta el vector.
fn first_then_push(v: &mut Vec<i32>) -> &i32 {
    let first = &v[0];
    v.push(42);
    first
}

// A user-defined struct with a lifetime.
struct Book<'a> {
    title: &'a str,
    pages: u32,
}

impl<'a> Book<'a> {
    fn new(title: &'a str) -> Self {
        Self { title, pages: 0 }
    }

    pub fn title(&self) -> &'a str {
        self.title
    }
}

fn main() -> Result<(), String> {
    let book = Book::new("Rust");
    println!("{}", book.title());

    let raw = r"C:\Users\src\file.rs";
    let raw2 = r#"JSON: {"ok": true}"#;
    let letter = 'A';
    let tab = '\t';
    let escape = "linea\nueva";

    let big: u64 = 0xFF;
    let size = 1_000_000usize;
    let byte: u8 = 42u8;
    let ratio: f64 = 3.14f64;
    let mask = 0b1010u16;

    let names: Vec<String> = vec!["alfa".to_string(), "beta".to_string()];
    match names.first() {
        Some(n) => println!("{n}"),
        None => println!("vacio"),
    }

    loop {
        break;
    }

    /* Comentario multilinea
       que cruza dos lineas */
    if size > 0 {
        return Ok(());
    }

    let opt: Option<i32> = None;
    let res: Result<Box<i32>, &'static str> = Err("error");
    unsafe { println!("{}", res.is_ok()); }
    Err(String::from("fin"))
}
```
