//  Some custom glyphs for the 1602 display
// All my own work; yay!

static byte onGlyph[8]      = { 0b00100,
                                0b01110,
                                0b11111,
                                0b10101,
                                0b00100,
                                0b00100,
                                0b00100,
                                0b00100 };

static byte offGlyph[8]     = { 0b00100,
                                0b00100,
                                0b00100,
                                0b00100,
                                0b10101,
                                0b11111,
                                0b01110,
                                0b00100 };
                               
static byte degreesGlyph[8] = { 0b01000,
                                0b10100,
                                0b01000,
                                0b00000,
                                0b00000,
                                0b00000,
                                0b00000,
                                0b00000 };

static byte fanGlyph[8]     = { 0b10101,
                                0b01110,
                                0b11011,
                                0b01110,
                                0b10101,
                                0b00000,
                                0b00000,
                                0b00000 };

static byte fan1Glyph[8]    = { 0b00000,
                                0b00000,
                                0b00100,
                                0b01100,
                                0b11100,
                                0b00000,
                                0b00000,
                                0b00000 };

static byte fan2Glyph[8]    = { 0b00000,
                                0b00010,
                                0b00110,
                                0b01110,
                                0b11110,
                                0b00000,
                                0b00000,
                                0b00000 };

static byte fan3Glyph[8]    = { 0b00001,
                                0b00011,
                                0b00111,
                                0b01111,
                                0b11111,
                                0b00000,
                                0b00000,
                                0b00000 };
