---
id: traits
title: Traits
sidebar_label: Traits
description: Advanced trait definitions, default implementations, and capabilities.
---

# Traits

Traits define capabilities that types can implement. They are the foundation of polymorphism in Zith.

## Basic Traits

### Built-in Traits

```zith
trait Copy {
    // Type is bitwise-copyable
}

trait Shared {
    // Safe to use in `share` contexts (thread-safe)
}

trait Lent {
    // Safe to borrow mutably from global `unique`
}

trait ToString {
    fn toString(self): string;
}
```

### Using Built-in Traits

```zith
struct Point { x: f32, y: f32 }

implement Copy for Point { }  // bitwise copy

implement ToString for Point {
    fn toString(self): string {
        "({}, {})".format(self.x, self.y)
    }
}
```

## Custom Traits

### Defining Traits

```zith
trait Drawable {
    fn draw(self: view, canvas: lend Canvas);
    fn bounds(self: view): Rect;
}

trait Serializable {
    fn serialize(self: view): []byte;
    fn deserialize(data: []byte): Self!;
}
```

### Implementing Traits

```zith
struct Circle {
    x: f32,
    y: f32,
    radius: f32
}

implement Drawable for Circle {
    fn draw(self: view, canvas: lend Canvas) {
        canvas.drawCircle(self.x, self.y, self.radius);
    }

    fn bounds(self: view): Rect {
        Rect {
            x: self.x - self.radius,
            y: self.y - self.radius,
            width: self.radius * 2,
            height: self.radius * 2
        }
    }
}
```

## Default Implementations

```zith
trait Printable {
    fn print(self: view) {
        println!("{}", self.toString());
    }

    fn toString(self: view): string;
}

implement Printable for i32 {
    fn toString(self: view): string {
        /* ... */
    }
}
```

## Trait Constraints

### Single Constraint

```zith
fn process<T: Copy>(item: T): T {
    item  // can copy
}
```

### Multiple Constraints

```zith
fn save<T: Shared + Serializable>(item: T) {
    let data = item.serialize();
    save_to_disk(data);
}
```

### Where Clauses

```zith
fn process_all<T>(items: []T) where T: Copy + ToString {
    for item in items {
        println!("{}", item.toString());
    }
}
```

## Associated Types

```zith
trait Container {
    type Item;

    fn get(self: view, index: usize): ?Item;
    fn add(self: lend item: Item);
    fn len(self: view): usize;
}

implement Container for Vec<T> {
    type Item = T;

    fn get(self: view, index: usize): ?T {
        if index < self.length {
            self.data[index]
        } else {
            null
        }
    }

    fn add(self: lend item: T) {
        self.push(item);
    }

    fn len(self: view): usize {
        self.length
    }
}
```

## Static vs Dynamic Dispatch

### Static (Monomorphization)

```zith
fn draw<T: Drawable>(obj: T, canvas: lend Canvas) {
    obj.draw(canvas);
}
// Compiles to specific type, zero overhead
```

### Dynamic (vtable)

```zith
fn draw_dyn(obj: Drawable*, canvas: lend Canvas) {
    obj.draw(canvas);
}
// Uses vtable, small runtime cost
```

## Trait Objects

```zith
// Reference to trait
let drawable: Drawable* = &circle;
drawable.draw(canvas);

// With erased type
let any: any = circle;
```

## Best Practices

- Define small, focused traits
- Use trait constraints for flexibility
- Prefer static dispatch for performance
- Use dynamic dispatch for heterogeneous collections

## See Also

- **[Generics](../language/generics.md)** - Generic types
- **[Interfaces](./how-to-use.md)** - Advanced patterns