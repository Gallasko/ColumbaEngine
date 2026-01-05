Script Language Overview
========================

The PgEngine script language is a dynamically-typed, bytecode-compiled scripting language designed for embedding into game engines and applications. It features a clean, C-like syntax with support for object-oriented programming, closures, and first-class functions.

Language Philosophy
-------------------

The script language is designed with the following goals:

* **Simplicity**: Easy to learn syntax similar to JavaScript and Lua
* **Performance**: Bytecode compilation with VM execution
* **Embeddability**: Native function registration and module system
* **Memory Safety**: Automatic garbage collection

Key Features
------------

Variables and Types
~~~~~~~~~~~~~~~~~~~

The language supports dynamic typing with the following types:

* **Numbers**: Integer and floating-point values
* **Strings**: UTF-8 text with concatenation support
* **Booleans**: ``true`` and ``false`` literals
* **Tables**: Associative arrays that can store key-value pairs and indexed values
* **Functions**: First-class functions with closure support
* **Classes**: Object-oriented programming with inheritance

Control Flow
~~~~~~~~~~~~

**Conditional Statements**

* ``if`` / ``else if`` / ``else`` statements
* Comparison operators: ``==``, ``!=``, ``<``, ``>``, ``<=``, ``>=``
* Logical operators: ``and``, ``or``, ``not``

**Loops**

* ``while`` loops
* ``for`` loops with initialization, condition, and increment
* ``for-in`` loops for iterating over tables

Functions
~~~~~~~~~

Functions are first-class citizens and support:

* Named and anonymous functions
* Closures with lexical scoping
* Multiple parameters and return values
* Nested function definitions

Example::

    fun greet(name)
    {
        return "Hello, " + name;
    }

    var message = greet("World");
    print(message);  // Output: Hello, World

Object-Oriented Programming
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The language supports class-based OOP with:

* Class declarations and instantiation
* Constructor methods (``init``)
* Instance methods and properties
* The ``this`` keyword for accessing instance members
* Method binding

Example::

    class CoffeeMaker
    {
        init(coffee)
        {
            this.coffee = coffee;
        }

        brew()
        {
            print("Enjoy your cup of " + this.coffee);
        }
    }

    var maker = CoffeeMaker("espresso");
    maker.brew();

Tables (Associative Arrays)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Tables are versatile data structures that combine arrays and dictionaries:

* Can store indexed values: ``[1, 2, 3]``
* Can store key-value pairs: ``[name: "Alice", age: 30]``
* Can mix both: ``[10, name: "Bob", 20]``
* Support dynamic access and modification

Example::

    var person = [name: "Emma", age: 23];
    print(person["name"]);  // Output: Emma

    var numbers = [5, 10, 15];
    print(numbers[0]);      // Output: 5

Module System
~~~~~~~~~~~~~

The language supports a module system with:

* ``import`` statements for loading external files
* Native module registration from C++
* Module search paths
* Compiled bytecode (.pgc) format

Built-in Native Modules:

* ``math``: Mathematical functions
* ``file``: File I/O operations
* ``string``: String manipulation utilities
* ``algorithm``: Common algorithms and data operations

Example::

    import "file"
    import "string"

    var data = readFile("data.txt");
    var lines = splitLines(data);

Operators
~~~~~~~~~

**Arithmetic**

* Addition: ``+``
* Subtraction: ``-``
* Multiplication: ``*``
* Division: ``/``
* Modulo: ``%``
* Unary negation: ``-x``

**Increment/Decrement**

* Prefix: ``++x``, ``--x``
* Postfix: ``x++``, ``x--``
* Compound assignment: ``+=``, ``-=``

**String Operations**

* Concatenation: ``"hello" + " world"``
* Character access: ``str[0]``, ``str[-1]`` (negative indexing)

**Logical**

* AND: ``and``
* OR: ``or``
* NOT: ``not``

Closures
~~~~~~~~

Functions can capture and maintain references to variables from their enclosing scope::

    fun outer()
    {
        var x = 1;
        fun inner()
        {
            print(x);  // Captures x from outer
        }

        return inner;
    }

    var closure = outer();
    closure();  // Output: 1

Compilation and Execution
--------------------------

Script files use the ``.pg`` extension and can be:

* **Interpreted** directly from source text
* **Compiled** to bytecode (``.pgc``) for faster loading
* **Imported** as modules in other scripts

The VM (Virtual Machine) executes the compiled bytecode with:

* Stack-based instruction execution
* Automatic garbage collection
* Native function interop

Debug Output
------------

The ``print()`` function is available for output in scripts.
The ``__dprint()`` function is available for debug output in scripts. It's commonly used in tests and development to print values to the console.

Getting Started
---------------

A simple "Hello, World!" program::

    print("Hello, World!");

A more complex example with functions and loops::

    fun fibonacci(n)
    {
        if (n <= 1)
        {
            return n;
        }

        return fibonacci(n - 1) + fibonacci(n - 2);
    }

    for (var i = 0; i < 10; i++)
    {
        print(fibonacci(i));
    }

Next Steps
----------

* See :doc:`api` for detailed API reference
* Explore the test scripts in ``test/pgcompiler/scripts/`` for more examples
* Check the Advent of Code examples in ``test/pgcompiler/examples/`` for real-world use cases
