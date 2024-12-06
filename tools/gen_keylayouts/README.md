# gen_cpp.sh

## env variables

- `KEYBOARD_JS_PATH`: Path of https://github.com/wallix/rdp-keyboard.js. Default is `../../../rdp-keyboard.js`
- `KBDLAYOUT_PATH`: Path of kbdlayout.info folder (see `rdp-keyboard.js` repository). Default is `$KEYBOARD_JS_PATH/tools/kbdlayout.info`

# Output Format

A layout is made up of 128 mod combinations (Ctrl, Shift, etc) representing a map of 255 keys. Plus a table of dead keys.

To reduce the amount of memory taken up by the layout, the parts are broken down and shared between all the layouts.

In this way, the 128 mods are divided into 4 blocks of 32 mods each. Each layout generally shares 3 empty blocks (KANA and OEM8 being present for only 2 layouts).

The keymap is split into 2, as the last 128 characters are almost always identical between layouts.

The keymap contains `uint16_t`, 14 bits of which are used to represent the unicode value. Bit 15 indicates an extended value (more than 2 bytes) whose continuation is read in the 3rd keymap group. Bit 16 indicates a dead key to be combined with the dead key table.
