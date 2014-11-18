Python Unicode Preprocessing
============================

Python's parser rejiggered to support Unicode operators.
* Input Python with Unicode '⊗' and '⊕' operators.
* Output Python with `.___otimes___` and `___oplus___` calls that respect Python's order of operations.

The following alterations have been made:
* The tokenizer's `tok_nextc` provides Unicode code points as `unsigned int`s.
* There's no linking with `PyObject` code.
* There's no file-based input.
* Emscripten exposes the functionality from within Javascript.
