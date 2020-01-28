# Scrib
Minimalistic Text Editor implemented using a Piece Table
Currently only supports insert/delete and undo/redo operations
Inspiration and some code from this website: https://viewsourcecode.org/snaptoken/kilo/index.html

## Usage
./scrib \<optional filename\>

## Currently Supported Operations:
* PageUp, PageDown: Scroll up/down
* Up/Down/Left/Right: Move cursor
* Home/End: move cursor to the beginning/end of editing line
* Ctrl-U Undo
* Ctrl-R Redo
* Ctrl-S: Save
* Ctrl-Q: Quit
* ESC to cancel search, Enter to exit search, arrows to navigate

## TODO:
* Cleaner Documentation
* Better Text Navigation
* Syntax Highlighting
* Search
* Copy/Paste
