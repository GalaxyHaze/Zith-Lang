---
id: stdlib
title: Standard Library
sidebar_label: Standard Library
description: Reference for the Zith standard library.
---

# Standard Library

The Zith standard library is organized into three parts.

## Library Structure

### `std` — Stable

Guaranteed backward compatibility.

```zith
import std;
```

### `utils` — Experimental

May change between versions.

```zith
import utils;
```

### `c` — C Bindings

FFI for C libraries.

```zith
import c;
```

## Common Modules

```zith
std/io/console{println, print, eprint}
std/collections{Vec, Map, Set}
std::thread::spawn, join
std::memory::allocate, deallocate
std::fs::File, open, read, write
std::json::parse, serialize
```

## Standard Traits

```zith
trait Copy { }
trait Shared { }
trait Lent { }
trait ToString { }
trait Drawable { }
```

## Collections

### `Vec<T>`

Dynamic array:

```zith
let vec = Vec::new();
vec.push(42);
let first = vec[0];
```

### `Map<K, V>`

Key-value store:

```zith
let map = Map::new();
map.insert("key", "value");
let val = map.get("key");
```

### `Set<T>`

Unique elements:

```zith
let set = Set::new();
set.insert(42);
```

## I/O

```zith
println("Hello");       // print to stdout
print("Value: {}", x); // formatted
eprint("Error");      // stderr
```

## Threading

```zith
std::thread::spawn(|| { /* work */ })
std::thread::join(handle)
```