---
id: modules
title: Modules & Contexts
sidebar_label: Modules
description: Learn about code organization, imports, and Contexts (DSLs).
---

# Modules & Contexts

Zith provides powerful tools for organizing code and creating domain-specific languages.

## Import System

### Import from Standard Library

```zith
import std;
import std.io;
import std.collections{Vec, Map};
```

### Import from Project

```zith
import mymodule;
import mymodule.submodule;
import mymodule { func1, func2 };
```

### Alias Imports

```zith
import std.io as io;
import very_long_module_name as vln;
```

### Selective Import with Visibility

```zith
import mymodule { pub func1, func2 };
// Only imports public functions
```

## Module System

### File as Module

Each `.zith` file is a module. The filename becomes the module name:

```zith
// src/utils/math.zith
pub fn add(a: i32, b: i32): i32 { a + b }
pub fn sub(a: i32, b: i32): i32 { a - b }

// Using
import utils.math;
let result = utils.math.add(1, 2);
```

### Module Declaration

```zith
mod mymodule {
    // internal code
    
    pub fn public_func() { }
    
    fn private_func() { }
}
```

## Contexts (DSL Namespaces)

Contexts are isolated environments that define valid syntax, keywords, and operators for domain-specific languages.

### Defining a Context

```zith
context WebApp {
    // Macros
    macro @getParam(name) {
        request.params[name]
    }
    
    // Constants
    const VERSION = "1.0";
    
    // Custom keywords (via macros)
    macro async { /* async handling */ }
    
    // Tag Macros (for HTML-like syntax)
    tag macro div(content) {
        render("<div>", content, "</div>")
    }
}
```

### Activating a Context

```zith
context WebApp {
    // Inside this block, WebApp syntax is active
    
    let version = VERSION;
    let param = @getParam("name");
    <div> Hello </div>
}

// Outside, WebApp syntax is not available
```

### Context Rules

- **One Global Context:** Only one "global" context active at module level
- **Local Contexts:** Can be opened locally in blocks
- **Namespace Isolation:** Context names don't pollute global namespace
- **Composition:** Can nest contexts

```zith
context Web {
    context Database {
        // Both Web and Database syntax available here
    }
}
```

### Tag Macros (DSL Syntax)

```zith
tag macro div(class: string, content) {
    "<div class=\"" + class + "\">" + content + "</div>"
}

context HTML {
    let page = <div class="container">
        <h1>Welcome</h1>
    </div>;
}
```

## Re-Exports

```zith
mod mymodule {
    pub fn helper() { }
    
    pub mod submodule {
        pub fn sub_helper() { }
    }
    
    // Re-export
    pub use helper;
    pub use submodule.*;
}
```

## Visibility Modifiers

```zith
pub          // public, accessible everywhere
pub(crate)   // public within crate
internal     // public within module
private      // only within current module
```

## Use Statement

```zith
use std.io;
use std.collections{Vec, Map};
use std as standard;
```

## Best Practices

- Use modules to organize related code
- Create contexts for domain-specific syntax
- Use clear, descriptive module names
- Keep contexts focused on specific domains

## Next Steps

- **[Contexts (Advanced)](./contexts.md)** - Deep dive into DSLs
- **[Syntax](./syntax.md)** - Language basics
- **[Standard Library](./reference/stdlib.md)** - Built-in modules