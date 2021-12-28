#include "Adafruit_SSD1306.h"
#include "splash.h"
#include "Adafruit_GFX.h"


#define ssd1306_swap(a, b)                                                     \
  (((a) ^= (b)), ((b) ^= (a)), ((a) ^= (b))) ///< No-temp-var swap operation

/*!
 * @file Adafruit_SSD1306.cpp
 *
 * @mainpage Arduino library for monochrome OLEDs based on SSD1306 drivers.
 *
 * @section intro_sec Introduction
 *
 * This is documentation for Adafruit's SSD1306 library for monochrome
 * OLED displays: http://www.adafruit.com/category/63_98
 *
 * These displays use I2C or SPI to communicate. I2C requires 2 pins
 * (SCL+SDA) and optionally a RESET pin. SPI requires 4 pins (MOSI, SCK,
 * select, data/command) and optionally a reset pin. Hardware SPI or
 * 'bitbang' software SPI are both supported.
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * @section dependencies Dependencies
 *
 * This library depends on <a
 * href="https://github.com/adafruit/Adafruit-GFX-Library"> Adafruit_GFX</a>
 * being present on your system. Please make sure you have installed the latest
 * version before using this library.
 *
 * @section author Author
 *
 * Written by Limor Fried/Ladyada for Adafruit Industries, with
 * contributions from the open source community.
 *
 * @section license License
 *
 * BSD license, all text above, and the splash screen included below,
 * must be included in any redistribution.
 *
 */



// CONSTRUCTORS, DESTRUCTOR ------------------------------------------------

/*!
    @brief  Constructor for I2C-interfaced SSD1306 displays.
    @param  w
            Display width in pixels
    @param  h
            Display height in pixels
    @return Adafruit_SSD1306 object.
    @note   Call the object's begin() function before use -- buffer
            allocation is performed there!
*/
Adafruit_SSD1306::Adafruit_SSD1306(uint8_t w, uint8_t h, i2c_port_t port) : Adafruit_GFX(w, h), buffer(NULL)
{
    i2c = port;
}

/*!
    @brief  Destructor for Adafruit_SSD1306 object.
*/
Adafruit_SSD1306::~Adafruit_SSD1306(void)
{
    if (buffer) {
        free(buffer);
        buffer = NULL;
    }
}


// ALLOCATE & INIT DISPLAY -------------------------------------------------

/*!
    @brief  Allocate RAM for image buffer, initialize peripherals and pins.
    @param  addr
            I2C address of corresponding SSD1306 display (or pass 0 to use
            default of 0x3C for 128x32 display, 0x3D for all others).
            SPI displays (hardware or software) do not use addresses, but
            this argument is still required (pass 0 or any value really,
            it will simply be ignored). Default if unspecified is 0.
    @return true on successful allocation/init, false otherwise.
            Well-behaved code should check the return value before
            proceeding.
    @note   MUST call this function before any drawing or updates!
*/
bool Adafruit_SSD1306::begin()
{
    if ((!buffer) && !(buffer = (uint8_t *)malloc(WIDTH * ((HEIGHT + 7) / 8)))) {
        return false;
    }

    clearDisplay();

#ifndef SSD1306_NO_SPLASH
    if (HEIGHT > 32) {
        drawBitmap((WIDTH - splash1_width) / 2, (HEIGHT - splash1_height) / 2,
                splash1_data, splash1_width, splash1_height, 1);
    }
    else {
        drawBitmap((WIDTH - splash2_width) / 2, (HEIGHT - splash2_height) / 2,
                splash2_data, splash2_width, splash2_height, 1);
    }
#endif

    // Init sequence
    static const uint8_t init1[] = {
        SSD1306_DISPLAYOFF,         // 0xAE
        SSD1306_SETDISPLAYCLOCKDIV, // 0xD5
        0x80,                       // the suggested ratio 0x80
        SSD1306_SETMULTIPLEX};      // 0xA8
    ssd1306_commandList(init1, sizeof(init1));
    
    ssd1306_command1(HEIGHT - 1);

    static const uint8_t init2[] = {
        SSD1306_SETDISPLAYOFFSET,   // 0xD3
        0x0,                        // no offset
        SSD1306_SETSTARTLINE | 0x0, // line #0
        SSD1306_CHARGEPUMP};        // 0x8D
    ssd1306_commandList(init2, sizeof(init2));

    ssd1306_command1(0x14);

    static const uint8_t init3[] = {
        SSD1306_MEMORYMODE,         // 0x20
        0x00,                       // 0x0 act like ks0108
        SSD1306_SEGREMAP | 0x1,
        SSD1306_COMSCANDEC};
    ssd1306_commandList(init3, sizeof(init3));

    uint8_t comPins = 0x02;
    contrast = 0x8F;

    if ((WIDTH == 128) && (HEIGHT == 32)) {
        comPins = 0x02;
        contrast = 0x8F;
    } 
    else if ((WIDTH == 128) && (HEIGHT == 64)) {
        comPins = 0x12;
        contrast = 0xCF;
    } 
    else if ((WIDTH == 96) && (HEIGHT == 16)) {
        comPins = 0x2; // ada x12
        contrast = 0xAF;
    } 
    else {
        // Other screen varieties -- TBD
    }

    ssd1306_command1(SSD1306_SETCOMPINS);
    ssd1306_command1(comPins);
    ssd1306_command1(SSD1306_SETCONTRAST);
    ssd1306_command1(contrast);


    ssd1306_command1(SSD1306_SETPRECHARGE); // 0xd9
    ssd1306_command1(0xF1);
    static const uint8_t init5[] = {
        SSD1306_SETVCOMDETECT, // 0xDB
        0x40,
        SSD1306_DISPLAYALLON_RESUME, // 0xA4
        SSD1306_NORMALDISPLAY,       // 0xA6
        SSD1306_DEACTIVATE_SCROLL,
        SSD1306_DISPLAYON}; // Main screen turn on
    ssd1306_commandList(init5, sizeof(init5));

    return (true);
}

/*!
    @brief Issue single command to SSD1306
   Because command calls are often grouped, SPI transaction and
   selection must be started/ended in calling function for efficiency. This is a
   protected function, not exposed (see ssd1306_command() instead).
        @param c
                   the command character to send to the display.
                   Refer to ssd1306 data sheet for commands
    @return None (void).
    @note
*/
void Adafruit_SSD1306::ssd1306_command1(uint8_t c)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if(cmd != NULL) {
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (SSD1306_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, SSD1306_CMD_STREAM, true);
        i2c_master_write_byte(cmd, c, true);
        i2c_master_stop(cmd);
        ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_cmd_begin(i2c, cmd, 10/portTICK_PERIOD_MS));
        i2c_cmd_link_delete(cmd);
    }
}

/*!
    @brief Issue list of commands to SSD1306, same rules as above re:
   transactions. This is a protected function, not exposed.
        @param c
                   pointer to list of commands
        @param n
                   number of commands in the list
    @return None (void).
    @note
*/
void Adafruit_SSD1306::ssd1306_commandList(const uint8_t *c, uint8_t n) {

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if(cmd != NULL) {
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (SSD1306_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, SSD1306_CMD_STREAM, true);
        while (n--) { i2c_master_write_byte(cmd, *c++, true); }
        i2c_master_stop(cmd);
        ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_cmd_begin(i2c, cmd, 10/portTICK_PERIOD_MS));
        i2c_cmd_link_delete(cmd);
    }
}

// DRAWING FUNCTIONS -------------------------------------------------------

/*!
    @brief  Set/clear/invert a single pixel. This is also invoked by the
            Adafruit_GFX library in generating many higher-level graphics
            primitives.
    @param  x
            Column of display -- 0 at left to (screen width - 1) at right.
    @param  y
            Row of display -- 0 at top to (screen height -1) at bottom.
    @param  color
            Pixel color, one of: SSD1306_BLACK, SSD1306_WHITE or
            SSD1306_INVERSE.
    @return None (void).
    @note   Changes buffer contents only, no immediate effect on display.
            Follow up with a call to display(), or with other graphics
            commands as needed by one's own application.
*/
void Adafruit_SSD1306::drawPixel(int16_t x, int16_t y, uint16_t color) 
{
    if ((x >= 0) && (x < width()) && (y >= 0) && (y < height())) {
        // Pixel is in-bounds. Rotate coordinates if needed.
        switch (getRotation()) {
        case 1:
            ssd1306_swap(x, y);
            x = WIDTH - x - 1;
            break;
        case 2:
            x = WIDTH - x - 1;
            y = HEIGHT - y - 1;
            break;
        case 3:
            ssd1306_swap(x, y);
            y = HEIGHT - y - 1;
            break;
        }
        switch (color) {
        case SSD1306_WHITE:
            buffer[x + (y / 8) * WIDTH] |= (1 << (y & 7));
            break;
        case SSD1306_BLACK:
            buffer[x + (y / 8) * WIDTH] &= ~(1 << (y & 7));
            break;
        case SSD1306_INVERSE:
            buffer[x + (y / 8) * WIDTH] ^= (1 << (y & 7));
            break;
        }
    }
}

/*!
    @brief  Clear contents of display buffer (set all pixels to off).
    @return None (void).
    @note   Changes buffer contents only, no immediate effect on display.
            Follow up with a call to display(), or with other graphics
            commands as needed by one's own application.
*/
void Adafruit_SSD1306::clearDisplay(void) 
{
    memset(buffer, 0, WIDTH * ((HEIGHT + 7) / 8));
}

/*!
    @brief  Return color of a single pixel in display buffer.
    @param  x
            Column of display -- 0 at left to (screen width - 1) at right.
    @param  y
            Row of display -- 0 at top to (screen height -1) at bottom.
    @return true if pixel is set (usually SSD1306_WHITE, unless display invert
   mode is enabled), false if clear (SSD1306_BLACK).
    @note   Reads from buffer contents; may not reflect current contents of
            screen if display() has not been called.
*/
bool Adafruit_SSD1306::getPixel(int16_t x, int16_t y)
{
    if ((x >= 0) && (x < width()) && (y >= 0) && (y < height())) {
        // Pixel is in-bounds. Rotate coordinates if needed.
        switch (getRotation()) {
        case 1:
            ssd1306_swap(x, y);
            x = WIDTH - x - 1;
            break;
        case 2:
            x = WIDTH - x - 1;
            y = HEIGHT - y - 1;
            break;
        case 3:
            ssd1306_swap(x, y);
            y = HEIGHT - y - 1;
            break;
        }
        return (buffer[x + (y / 8) * WIDTH] & (1 << (y & 7)));
    }
    return false; // Pixel out of bounds
}

// A public version of ssd1306_command1(), for existing user code that
// might rely on that function. This encapsulates the command transfer
// in a transaction start/end, similar to old library's handling of it.
/*!
    @brief  Issue a single low-level command directly to the SSD1306
            display, bypassing the library.
    @param  c
            Command to issue (0x00 to 0xFF, see datasheet).
    @return None (void).
*/
void Adafruit_SSD1306::ssd1306_command(uint8_t c) {
    ssd1306_command1(c);
}

/*!
    @brief  Get base address of display buffer for direct reading or writing.
    @return Pointer to an unsigned 8-bit array, column-major, columns padded
            to full byte boundary if needed.
*/
uint8_t *Adafruit_SSD1306::getBuffer(void) { return buffer; }

// REFRESH DISPLAY ---------------------------------------------------------

/*!
    @brief  Push data currently in RAM to SSD1306 display.
    @return None (void).
    @note   Drawing operations are not visible until this function is
            called. Call after each graphics command, or after a whole set
            of graphics commands, as best needed by one's own application.
*/
void Adafruit_SSD1306::display(void)
{
    static const uint8_t dlist1[] = {
        SSD1306_PAGEADDR,
        0,                      // Page start address
        0xFF,                   // Page end (not really, but works here)
        SSD1306_COLUMNADDR, 0}; // Column start address
    ssd1306_commandList(dlist1, sizeof(dlist1));
    ssd1306_command1(WIDTH - 1); // Column end address

    uint16_t count = WIDTH * ((HEIGHT + 7) / 8);
    uint8_t *ptr = buffer;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (SSD1306_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, SSD1306_DATA_STREAM, true);

    while (count--) {
        i2c_master_write_byte(cmd, *ptr++, true);
    }

    i2c_master_stop(cmd);
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_cmd_begin(i2c, cmd, 10/portTICK_PERIOD_MS));
    i2c_cmd_link_delete(cmd);
}

// SCROLLING FUNCTIONS -----------------------------------------------------

/*!
    @brief  Activate a right-handed scroll for all or part of the display.
    @param  start
            First row.
    @param  stop
            Last row.
    @return None (void).
*/
// To scroll the whole display, run: display.startscrollright(0x00, 0x0F)
void Adafruit_SSD1306::startscrollright(uint8_t start, uint8_t stop) 
{
    static const uint8_t scrollList1a[] = {
        SSD1306_RIGHT_HORIZONTAL_SCROLL, 
        0X00
    };
    ssd1306_commandList(scrollList1a, sizeof(scrollList1a));
    ssd1306_command1(start);
    ssd1306_command1(0X00);
    ssd1306_command1(stop);
    static const uint8_t scrollList1b[] = {
        0X00, 
        0XFF,
        SSD1306_ACTIVATE_SCROLL
    };
    ssd1306_commandList(scrollList1b, sizeof(scrollList1b));
}

/*!
    @brief  Activate a left-handed scroll for all or part of the display.
    @param  start
            First row.
    @param  stop
            Last row.
    @return None (void).
*/
// To scroll the whole display, run: display.startscrollleft(0x00, 0x0F)
void Adafruit_SSD1306::startscrollleft(uint8_t start, uint8_t stop)
{
    static const uint8_t scrollList2a[] = {
        SSD1306_LEFT_HORIZONTAL_SCROLL,
        0X00
    };
    ssd1306_commandList(scrollList2a, sizeof(scrollList2a));
    ssd1306_command1(start);
    ssd1306_command1(0X00);
    ssd1306_command1(stop);
    static const uint8_t scrollList2b[] = {
        0X00, 
        0XFF,
        SSD1306_ACTIVATE_SCROLL
    };
    ssd1306_commandList(scrollList2b, sizeof(scrollList2b));
}

/*!
    @brief  Activate a diagonal scroll for all or part of the display.
    @param  start
            First row.
    @param  stop
            Last row.
    @return None (void).
*/
// display.startscrolldiagright(0x00, 0x0F)
void Adafruit_SSD1306::startscrolldiagright(uint8_t start, uint8_t stop)
{
    static const uint8_t scrollList3a[] = {
        SSD1306_SET_VERTICAL_SCROLL_AREA, 
        0X00
    };
    ssd1306_commandList(scrollList3a, sizeof(scrollList3a));
    ssd1306_command1(HEIGHT);
    static const uint8_t scrollList3b[] = {
        SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL, 
        0X00
    };
    ssd1306_commandList(scrollList3b, sizeof(scrollList3b));
    ssd1306_command1(start);
    ssd1306_command1(0X00);
    ssd1306_command1(stop);
    static const uint8_t scrollList3c[] = {
        0X01, 
        SSD1306_ACTIVATE_SCROLL
    };
    ssd1306_commandList(scrollList3c, sizeof(scrollList3c));
}

/*!
    @brief  Activate alternate diagonal scroll for all or part of the display.
    @param  start
            First row.
    @param  stop
            Last row.
    @return None (void).
*/
// To scroll the whole display, run: display.startscrolldiagleft(0x00, 0x0F)
void Adafruit_SSD1306::startscrolldiagleft(uint8_t start, uint8_t stop)
{
    static const uint8_t scrollList4a[] = {
        SSD1306_SET_VERTICAL_SCROLL_AREA, 
        0X00
    };
    ssd1306_commandList(scrollList4a, sizeof(scrollList4a));
    ssd1306_command1(HEIGHT);
    static const uint8_t scrollList4b[] = {
        SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL, 
        0X00
    };
    ssd1306_commandList(scrollList4b, sizeof(scrollList4b));
    ssd1306_command1(start);
    ssd1306_command1(0X00);
    ssd1306_command1(stop);
    static const uint8_t scrollList4c[] = {
        0X01, 
        SSD1306_ACTIVATE_SCROLL
    };
    ssd1306_commandList(scrollList4c, sizeof(scrollList4c));
}

/*!
    @brief  Cease a previously-begun scrolling action.
    @return None (void).
*/
void Adafruit_SSD1306::stopscroll(void)
{
    ssd1306_command1(SSD1306_DEACTIVATE_SCROLL);
}


// OTHER HARDWARE SETTINGS -------------------------------------------------

/*!
    @brief  Enable or disable display invert mode (white-on-black vs
            black-on-white).
    @param  i
            If true, switch to invert mode (black-on-white), else normal
            mode (white-on-black).
    @return None (void).
    @note   This has an immediate effect on the display, no need to call the
            display() function -- buffer contents are not changed, rather a
            different pixel mode of the display hardware is used. When
            enabled, drawing SSD1306_BLACK (value 0) pixels will actually draw
   white, SSD1306_WHITE (value 1) will draw black.
*/
void Adafruit_SSD1306::invertDisplay(bool i) {
    ssd1306_command1(i ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);
}

/*!
    @brief  Dim the display.
    @param  dim
            true to enable lower brightness mode, false for full brightness.
    @return None (void).
    @note   This has an immediate effect on the display, no need to call the
            display() function -- buffer contents are not changed.
*/
void Adafruit_SSD1306::dim(bool dim) {
    // the range of contrast to too small to be really useful
    // it is useful to dim the display
    ssd1306_command1(SSD1306_SETCONTRAST);
    ssd1306_command1(dim ? 0 : contrast);
}